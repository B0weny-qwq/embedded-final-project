mod app;
mod plot;
mod protocol;
mod serial;
mod state;

use app::TempMonitorApp;

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([1100.0, 720.0]),
        ..Default::default()
    };
    eframe::run_native(
        "STM32 Temperature Monitor",
        options,
        Box::new(|cc| Box::new(TempMonitorApp::new(cc))),
    )
}
