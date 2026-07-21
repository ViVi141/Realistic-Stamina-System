pub fn clip_f64(value: f64, min_v: f64, max_v: f64) -> f64 {
    value.max(min_v).min(max_v)
}
