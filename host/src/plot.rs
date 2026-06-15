use egui::Ui;
use egui_plot::{Legend, Line, Plot, PlotPoints};

use crate::state::AppState;

pub fn temperature_plot(ui: &mut Ui, state: &AppState) {
    let raw = PlotPoints::from_iter(state.samples.iter().map(|p| [p.t_s, p.raw_c]));
    let filtered = PlotPoints::from_iter(state.samples.iter().map(|p| [p.t_s, p.filtered_c]));

    Plot::new("temperature_plot")
        .legend(Legend::default())
        .height(360.0)
        .allow_scroll(false)
        .allow_boxed_zoom(false)
        .show(ui, |plot_ui| {
            plot_ui.line(Line::new(raw).name("raw C"));
            plot_ui.line(Line::new(filtered).name("filtered C"));
        });
}
