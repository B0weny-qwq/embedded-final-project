#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LedMode {
    Auto,
    Off,
    Red,
    Green,
    Blue,
    White,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BeepMode {
    Auto,
    On,
    Off,
}

#[derive(Clone, Debug)]
pub struct Telemetry {
    pub ms: u64,
    pub raw_c: f64,
    pub filtered_c: f64,
    pub alarm: bool,
    pub led: LedMode,
    pub beep: BeepMode,
}

impl LedMode {
    pub const ALL: [LedMode; 6] = [
        LedMode::Auto,
        LedMode::Off,
        LedMode::Red,
        LedMode::Green,
        LedMode::Blue,
        LedMode::White,
    ];

    pub fn as_str(self) -> &'static str {
        match self {
            LedMode::Auto => "AUTO",
            LedMode::Off => "OFF",
            LedMode::Red => "RED",
            LedMode::Green => "GREEN",
            LedMode::Blue => "BLUE",
            LedMode::White => "WHITE",
        }
    }
}

impl BeepMode {
    pub const ALL: [BeepMode; 3] = [BeepMode::Auto, BeepMode::On, BeepMode::Off];

    pub fn as_str(self) -> &'static str {
        match self {
            BeepMode::Auto => "AUTO",
            BeepMode::On => "ON",
            BeepMode::Off => "OFF",
        }
    }
}

fn parse_led(value: &str) -> Option<LedMode> {
    match value {
        "AUTO" => Some(LedMode::Auto),
        "OFF" => Some(LedMode::Off),
        "RED" => Some(LedMode::Red),
        "GREEN" => Some(LedMode::Green),
        "BLUE" => Some(LedMode::Blue),
        "WHITE" => Some(LedMode::White),
        _ => None,
    }
}

fn parse_beep(value: &str) -> Option<BeepMode> {
    match value {
        "AUTO" => Some(BeepMode::Auto),
        "ON" => Some(BeepMode::On),
        "OFF" => Some(BeepMode::Off),
        _ => None,
    }
}

pub fn parse_telemetry(line: &str) -> Option<Telemetry> {
    let parts: Vec<_> = line.trim().split(',').collect();
    if parts.len() != 7 || parts[0] != "T" {
        return None;
    }

    Some(Telemetry {
        ms: parts[1].parse().ok()?,
        raw_c: parts[2].parse::<f64>().ok()? / 100.0,
        filtered_c: parts[3].parse::<f64>().ok()? / 100.0,
        alarm: parts[4] == "1",
        led: parse_led(parts[5])?,
        beep: parse_beep(parts[6])?,
    })
}

pub fn cmd_threshold(celsius: f64) -> String {
    format!("CMD,TH,{:.0}\n", celsius * 100.0)
}

pub fn cmd_led(mode: LedMode) -> String {
    format!("CMD,LED,{}\n", mode.as_str())
}

pub fn cmd_beep(mode: BeepMode) -> String {
    format!("CMD,BEEP,{}\n", mode.as_str())
}

pub fn cmd_ack() -> &'static str {
    "CMD,ACK\n"
}
