use eframe::egui;

use crate::plot::temperature_plot;
use crate::protocol::{cmd_ack, cmd_beep, cmd_led, cmd_threshold, BeepMode, LedMode};
use crate::serial::{available_ports, SerialController, SerialEvent};
use crate::state::AppState;

pub struct TempMonitorApp {
    state: AppState,
    serial: SerialController,
    selected_port: String,
    ports: Vec<String>,
}

impl TempMonitorApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        let ports = available_ports();
        let selected_port = ports
            .iter()
            .find(|p| p.starts_with("/dev/ttyACM"))
            .cloned()
            .or_else(|| ports.first().cloned())
            .unwrap_or_default();
        Self {
            state: AppState::new(),
            serial: SerialController::spawn(if selected_port.is_empty() {
                None
            } else {
                Some(selected_port.clone())
            }),
            selected_port,
            ports,
        }
    }

    fn handle_serial_events(&mut self) {
        for event in self.serial.drain_events() {
            match event {
                SerialEvent::Connected(port) => {
                    self.state.connected_port = Some(port);
                    self.state.last_error = None;
                }
                SerialEvent::Disconnected(reason) => {
                    self.state.connected_port = None;
                    self.state.last_error = Some(reason);
                }
                SerialEvent::Telemetry(telemetry) => self.state.apply_telemetry(telemetry),
                SerialEvent::Error(error) => self.state.last_error = Some(error),
            }
        }
    }

    fn top_bar(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            if ui.button("Refresh").clicked() {
                self.ports = available_ports();
            }

            egui::ComboBox::from_id_source("ports")
                .selected_text(if self.selected_port.is_empty() {
                    "auto"
                } else {
                    &self.selected_port
                })
                .show_ui(ui, |ui| {
                    for port in &self.ports {
                        ui.selectable_value(&mut self.selected_port, port.clone(), port);
                    }
                });

            if ui.button("Connect").clicked() && !self.selected_port.is_empty() {
                self.serial.send(format!("@PORT:{}\n", self.selected_port));
            }

            let status = if self.state.is_online() {
                format!(
                    "online: {}",
                    self.state.connected_port.as_deref().unwrap_or("unknown")
                )
            } else {
                self.state
                    .last_error
                    .clone()
                    .unwrap_or_else(|| "waiting for device".into())
            };
            ui.label(status);
        });
    }

    fn controls(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label("Threshold");
            ui.add(
                egui::DragValue::new(&mut self.state.threshold_c)
                    .speed(0.25)
                    .suffix(" C"),
            );
            if ui.button("Apply").clicked() {
                self.serial.send(cmd_threshold(self.state.threshold_c));
            }

            ui.separator();
            ui.label("LED");
            for mode in LedMode::ALL {
                if ui
                    .selectable_label(self.state.led_mode == mode, mode.as_str())
                    .clicked()
                {
                    self.state.led_mode = mode;
                    self.serial.send(cmd_led(mode));
                }
            }
        });

        ui.horizontal(|ui| {
            ui.label("Buzzer");
            for mode in BeepMode::ALL {
                if ui
                    .selectable_label(self.state.beep_mode == mode, mode.as_str())
                    .clicked()
                {
                    self.state.beep_mode = mode;
                    self.serial.send(cmd_beep(mode));
                }
            }
            ui.separator();
            if ui.button("ACK Silence").clicked() {
                self.serial.send(cmd_ack());
            }
        });
    }

    fn metrics(&self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            if let Some(latest) = &self.state.latest {
                ui.heading(format!("{:.2} C", latest.filtered_c));
                ui.label(format!("raw {:.2} C", latest.raw_c));
                ui.label(format!("threshold {:.2} C", self.state.threshold_c));
                ui.label(format!("LED {}", latest.led.as_str()));
                ui.label(format!("buzzer {}", latest.beep.as_str()));
            } else {
                ui.heading("No samples");
            }
        });
    }
}

impl eframe::App for TempMonitorApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.handle_serial_events();
        ctx.request_repaint_after(std::time::Duration::from_millis(50));

        egui::TopBottomPanel::top("top").show(ctx, |ui| {
            self.top_bar(ui);
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            if self.state.latest.as_ref().map(|t| t.alarm).unwrap_or(false) {
                egui::Frame::none()
                    .fill(egui::Color32::from_rgb(120, 24, 24))
                    .inner_margin(egui::Margin::same(10.0))
                    .show(ui, |ui| {
                        ui.heading("ALARM: filtered temperature is above threshold");
                    });
                ui.add_space(8.0);
            }

            self.metrics(ui);
            ui.add_space(8.0);
            self.controls(ui);
            ui.separator();
            temperature_plot(ui, &self.state);
        });
    }
}
