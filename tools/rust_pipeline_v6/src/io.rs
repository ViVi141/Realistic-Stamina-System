use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::Path;

use serde_json::{Map, Value};

pub fn read_json_object(path: &Path) -> Result<Map<String, Value>, String> {
    let text =
        fs::read_to_string(path).map_err(|e| format!("failed to read {}: {e}", path.display()))?;
    let v: Value = serde_json::from_str(&text)
        .map_err(|e| format!("failed to parse {}: {e}", path.display()))?;
    match v {
        Value::Object(map) => Ok(map),
        _ => Err(format!("{} is not a JSON object", path.display())),
    }
}

pub fn snapshot_value_types(data: &Map<String, Value>) -> BTreeMap<String, String> {
    let mut out = BTreeMap::new();
    for (k, v) in data {
        let kind = match v {
            Value::Null => "null",
            Value::Bool(_) => "bool",
            Value::Number(_) => "number",
            Value::String(_) => "string",
            Value::Array(_) => "array",
            Value::Object(_) => "object",
        };
        out.insert(k.clone(), kind.to_string());
    }
    out
}

pub fn merge_key_types(
    merged: &mut BTreeMap<String, BTreeSet<String>>,
    per_file_types: &BTreeMap<String, String>,
) {
    for (k, kind) in per_file_types {
        merged.entry(k.clone()).or_default().insert(kind.clone());
    }
}
