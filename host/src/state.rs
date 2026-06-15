use std::collections::VecDeque;
use std::time::{Duration, Instant};

use crate::protocol::{BeepMode, LedMode, Telemetry};

const MAX_POINTS: usize = 1800;

#[derive(Clone, Copy, Debug)]
pub struct SamplePoint {
    pub t_s: f64,
    pub raw_c: f64,
    pub filtered_c: f64,
}

pub struct AppState {
    pub connected_port: Option<String>,
    pub last_error: Option<String>,
    pub last_seen: Option<Instant>,
    pub samples: VecDeque<SamplePoint>,
    pub latest: Option<Telemetry>,
    pub threshold_c: f64,
    pub led_mode: LedMode,
    pub beep_mode: BeepMode,
}

impl AppState {
    pub fn new() -> Self {
        Self {
            connected_port: None,
            last_error: None,
            last_seen: None,
            samples: VecDeque::with_capacity(MAX_POINTS),
            latest: None,
            threshold_c: 45.0,
            led_mode: LedMode::Auto,
            beep_mode: BeepMode::Auto,
        }
    }

    pub fn apply_telemetry(&mut self, telemetry: Telemetry) {
        let point = SamplePoint {
            t_s: telemetry.ms as f64 / 1000.0,
            raw_c: telemetry.raw_c,
            filtered_c: telemetry.filtered_c,
        };
        self.samples.push_back(point);
        while self.samples.len() > MAX_POINTS {
            self.samples.pop_front();
        }
        self.led_mode = telemetry.led;
        self.beep_mode = telemetry.beep;
        self.last_seen = Some(Instant::now());
        self.latest = Some(telemetry);
    }

    pub fn is_online(&self) -> bool {
        self.last_seen
            .map(|seen| seen.elapsed() < Duration::from_secs(3))
            .unwrap_or(false)
    }
}
