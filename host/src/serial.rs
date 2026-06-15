use std::io::{Read, Write};
use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;
use std::time::{Duration, Instant};

use crate::protocol::{parse_telemetry, Telemetry};

#[derive(Debug)]
pub enum SerialEvent {
    Connected(String),
    Disconnected(String),
    Telemetry(Telemetry),
    Error(String),
}

pub struct SerialController {
    command_tx: Sender<String>,
    event_rx: Receiver<SerialEvent>,
}

impl SerialController {
    pub fn spawn(initial_port: Option<String>) -> Self {
        let (command_tx, command_rx) = mpsc::channel::<String>();
        let (event_tx, event_rx) = mpsc::channel::<SerialEvent>();
        thread::spawn(move || serial_thread(initial_port, command_rx, event_tx));
        Self { command_tx, event_rx }
    }

    pub fn send(&self, command: impl Into<String>) {
        let _ = self.command_tx.send(command.into());
    }

    pub fn drain_events(&self) -> Vec<SerialEvent> {
        let mut events = Vec::new();
        while let Ok(event) = self.event_rx.try_recv() {
            events.push(event);
        }
        events
    }
}

pub fn available_ports() -> Vec<String> {
    let mut ports = serialport::available_ports()
        .map(|ports| ports.into_iter().map(|p| p.port_name).collect::<Vec<_>>())
        .unwrap_or_default();
    ports.sort();
    ports
}

fn choose_port(preferred: &Option<String>) -> Option<String> {
    let ports = available_ports();
    if let Some(preferred) = preferred {
        if ports.iter().any(|p| p == preferred) {
            return Some(preferred.clone());
        }
    }
    ports
        .iter()
        .find(|p| p.starts_with("/dev/ttyACM"))
        .cloned()
        .or_else(|| ports.first().cloned())
}

fn serial_thread(
    mut preferred_port: Option<String>,
    command_rx: Receiver<String>,
    event_tx: Sender<SerialEvent>,
) {
    loop {
        while let Ok(command) = command_rx.try_recv() {
            if let Some(rest) = command.strip_prefix("@PORT:") {
                preferred_port = Some(rest.trim().to_owned());
            }
        }

        let Some(port_name) = choose_port(&preferred_port) else {
            let _ = event_tx.send(SerialEvent::Disconnected("waiting for /dev/ttyACM*".into()));
            thread::sleep(Duration::from_secs(1));
            continue;
        };

        let port_result = serialport::new(&port_name, 115_200)
            .timeout(Duration::from_millis(20))
            .open();

        let Ok(mut port) = port_result else {
            let _ = event_tx.send(SerialEvent::Error(format!("failed to open {port_name}")));
            thread::sleep(Duration::from_secs(1));
            continue;
        };

        let _ = event_tx.send(SerialEvent::Connected(port_name.clone()));
        let mut line = Vec::with_capacity(128);
        let mut buf = [0u8; 64];
        let mut last_rx = Instant::now();
        let mut reconnect = false;

        loop {
            for command in command_rx.try_iter() {
                if let Some(rest) = command.strip_prefix("@PORT:") {
                    preferred_port = Some(rest.trim().to_owned());
                    reconnect = true;
                    break;
                }
                if let Err(err) = port.write_all(command.as_bytes()) {
                    let _ = event_tx.send(SerialEvent::Error(format!("write failed: {err}")));
                    reconnect = true;
                    break;
                }
            }
            if reconnect {
                break;
            }

            match port.read(&mut buf) {
                Ok(0) => {}
                Ok(n) => {
                    last_rx = Instant::now();
                    for &byte in &buf[..n] {
                        if byte == b'\n' || byte == b'\r' {
                            if !line.is_empty() {
                                if let Ok(text) = std::str::from_utf8(&line) {
                                    if let Some(telemetry) = parse_telemetry(text) {
                                        let _ = event_tx.send(SerialEvent::Telemetry(telemetry));
                                    }
                                }
                                line.clear();
                            }
                        } else if line.len() < 160 {
                            line.push(byte);
                        } else {
                            line.clear();
                        }
                    }
                }
                Err(ref err) if err.kind() == std::io::ErrorKind::TimedOut => {}
                Err(err) => {
                    let _ = event_tx.send(SerialEvent::Disconnected(format!("{port_name}: {err}")));
                    break;
                }
            }

            if last_rx.elapsed() > Duration::from_secs(10) {
                let _ = event_tx.send(SerialEvent::Disconnected(format!("{port_name}: no data")));
                break;
            }
        }
    }
}
