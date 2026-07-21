#![allow(dead_code)]

#[derive(Debug, Clone)]
pub struct ConstraintCheck {
    pub name: String,
    pub passed: bool,
    pub detail: String,
    pub hard: bool,
}

#[derive(Debug, Clone, Default)]
pub struct ConstraintReport {
    pub checks: Vec<ConstraintCheck>,
}

impl ConstraintReport {
    pub fn all_hard_passed(&self) -> bool {
        self.checks.iter().all(|c| !c.hard || c.passed)
    }
}
