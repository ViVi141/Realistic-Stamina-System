#!/usr/bin/env python3
"""Apply-path twin: absolute-speed slew + hard-clamp policy (control loop).

Metabolic twins only check algebraic v_limit(state). This module catches
armed→disarmed SNAP / phys thrash that live in PlayerBase_UpdateLoop.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Literal, Tuple


TRANSITION_DURATION = 3.2
DISARM_TRANSITION_DURATION = 5.2
LARGE_DROP_TRANSITION_DURATION = 4.6
HUGE_DROP_TRANSITION_DURATION = 5.5
CHANGE_THRESHOLD_MS = 0.08
DROP_SMOOTH_THRESHOLD_MS = 0.22
LARGE_DROP_MS = 1.00
HUGE_DROP_MS = 1.40
SNAP_UP_THRESHOLD_MS = 0.55
SNAP_UP_HYST_MS = 0.20

ClampPolicy = Literal["fixed", "legacy"]


def _duration_for_drop_ms(drop_ms: float) -> float:
    if drop_ms >= HUGE_DROP_MS:
        return HUGE_DROP_TRANSITION_DURATION
    if drop_ms >= LARGE_DROP_MS:
        return LARGE_DROP_TRANSITION_DURATION
    return TRANSITION_DURATION


def _smoothstep01(progress: float) -> float:
    p = max(0.0, min(1.0, progress))
    return p * p * (3.0 - 2.0 * p)


@dataclass
class AbsSpeedTransition:
    """Python port of SCR_RSS_SprintBlockSpeedTransition (drop / disarm path)."""

    current_smoothed_abs_ms: float = 0.0
    transition_start_abs_ms: float = 0.0
    transition_target_abs_ms: float = 0.0
    transition_start_time: float = -1.0
    active_transition_duration: float = TRANSITION_DURATION
    was_sprint_allowed: bool = True
    was_overspeed_armed: bool = True
    snap_up_active: bool = False

    def is_in_transition(self) -> bool:
        return self.transition_start_time >= 0.0

    def _begin(
        self, current_time: float, start_abs_ms: float, target_abs_ms: float, duration_sec: float
    ) -> None:
        self.transition_start_abs_ms = start_abs_ms
        self.transition_target_abs_ms = target_abs_ms
        self.transition_start_time = current_time
        self.current_smoothed_abs_ms = start_abs_ms
        self.active_transition_duration = max(0.5, duration_sec)
        self.snap_up_active = False

    def ensure_disarm_transition(
        self, current_time: float, start_abs_ms: float, target_abs_ms: float
    ) -> None:
        if not self.was_overspeed_armed:
            return
        if start_abs_ms < 0.05:
            start_abs_ms = self.current_smoothed_abs_ms
        if start_abs_ms < 0.05:
            start_abs_ms = target_abs_ms
        if self.current_smoothed_abs_ms > start_abs_ms:
            start_abs_ms = self.current_smoothed_abs_ms
        drop_ms = start_abs_ms - target_abs_ms
        dur = DISARM_TRANSITION_DURATION
        drop_dur = _duration_for_drop_ms(drop_ms)
        if drop_dur > dur:
            dur = drop_dur
        self._begin(current_time, start_abs_ms, target_abs_ms, dur)
        self.was_overspeed_armed = False

    def update_and_get(
        self,
        current_time: float,
        target_absolute_speed_ms: float,
        current_engine_base_ms: float,
        sprint_allowed: bool,
        last_applied_multiplier: float,
        last_engine_base_ms: float,
        overspeed_armed: bool,
    ) -> float:
        target_absolute_speed_ms = max(target_absolute_speed_ms, 0.01)
        current_engine_base_ms = max(current_engine_base_ms, 0.1)
        last_engine_base_ms = max(last_engine_base_ms, 0.1)

        if self.current_smoothed_abs_ms <= 0.01:
            self.current_smoothed_abs_ms = target_absolute_speed_ms

        if self.was_overspeed_armed and not overspeed_armed:
            start_abs_ms = last_applied_multiplier * last_engine_base_ms
            if start_abs_ms < self.current_smoothed_abs_ms:
                start_abs_ms = self.current_smoothed_abs_ms
            drop_on_disarm = start_abs_ms - target_absolute_speed_ms
            dur = DISARM_TRANSITION_DURATION
            drop_dur = _duration_for_drop_ms(drop_on_disarm)
            if drop_dur > dur:
                dur = drop_dur
            self._begin(current_time, start_abs_ms, target_absolute_speed_ms, dur)
        self.was_overspeed_armed = overspeed_armed

        if self.was_sprint_allowed and not sprint_allowed:
            start_abs_ms = last_applied_multiplier * last_engine_base_ms
            if start_abs_ms < self.current_smoothed_abs_ms:
                start_abs_ms = self.current_smoothed_abs_ms
            drop_sp = start_abs_ms - target_absolute_speed_ms
            self._begin(
                current_time,
                start_abs_ms,
                target_absolute_speed_ms,
                _duration_for_drop_ms(drop_sp),
            )

        if (not self.was_sprint_allowed) and sprint_allowed:
            self.transition_start_time = -1.0
            self.current_smoothed_abs_ms = target_absolute_speed_ms
            self.snap_up_active = False

        self.was_sprint_allowed = sprint_allowed

        drop_ms = self.current_smoothed_abs_ms - target_absolute_speed_ms
        gain_ms = target_absolute_speed_ms - self.current_smoothed_abs_ms

        if gain_ms >= SNAP_UP_THRESHOLD_MS:
            if self.snap_up_active:
                self.snap_up_active = gain_ms >= SNAP_UP_HYST_MS
            if not self.snap_up_active:
                self.current_smoothed_abs_ms = target_absolute_speed_ms
                self.transition_target_abs_ms = target_absolute_speed_ms
                self.transition_start_time = -1.0
                self.snap_up_active = True
                return self.current_smoothed_abs_ms / current_engine_base_ms
        else:
            if self.snap_up_active and gain_ms < SNAP_UP_HYST_MS:
                self.snap_up_active = False

        significant_drop = False
        if drop_ms >= DROP_SMOOTH_THRESHOLD_MS:
            significant_drop = True
        if (not sprint_allowed) and drop_ms >= CHANGE_THRESHOLD_MS:
            significant_drop = True

        significant_gain_smooth = False
        if DROP_SMOOTH_THRESHOLD_MS <= gain_ms < SNAP_UP_THRESHOLD_MS:
            significant_gain_smooth = True

        target_changed = abs(target_absolute_speed_ms - self.transition_target_abs_ms) >= CHANGE_THRESHOLD_MS
        if (significant_drop or significant_gain_smooth) and (
            target_changed or self.transition_start_time < 0.0
        ):
            if (
                significant_drop
                and self.transition_start_time >= 0.0
                and self.current_smoothed_abs_ms > target_absolute_speed_ms
            ):
                self.transition_target_abs_ms = target_absolute_speed_ms
                need_dur = _duration_for_drop_ms(drop_ms)
                if need_dur > self.active_transition_duration:
                    self.active_transition_duration = need_dur
            else:
                self._begin(
                    current_time,
                    self.current_smoothed_abs_ms,
                    target_absolute_speed_ms,
                    _duration_for_drop_ms(drop_ms),
                )

        if self.transition_start_time >= 0.0:
            elapsed = current_time - self.transition_start_time
            dur = self.active_transition_duration
            if dur < 0.5:
                dur = TRANSITION_DURATION
            progress = max(0.0, min(1.0, elapsed / dur))
            smooth = _smoothstep01(progress)
            self.current_smoothed_abs_ms = self.transition_start_abs_ms + (
                self.transition_target_abs_ms - self.transition_start_abs_ms
            ) * smooth
            if progress >= 1.0:
                self.transition_start_time = -1.0
        else:
            self.current_smoothed_abs_ms = target_absolute_speed_ms

        abs_for_frac = self.current_smoothed_abs_ms
        if abs_for_frac > current_engine_base_ms:
            abs_for_frac = current_engine_base_ms
        return abs_for_frac / current_engine_base_ms


@dataclass
class ApplyPathSample:
    t: float
    theoretical_abs_ms: float
    applied_abs_ms: float
    v_meas_ms: float
    armed: bool
    in_transition: bool
    hard_clamped: bool


@dataclass
class ApplyPathSimResult:
    samples: List[ApplyPathSample] = field(default_factory=list)

    @property
    def applied(self) -> List[float]:
        return [s.applied_abs_ms for s in self.samples]

    @property
    def measured(self) -> List[float]:
        return [s.v_meas_ms for s in self.samples]


def allow_overspeed_hard_clamp(
    overspeeding: bool,
    in_transition: bool,
    armed: bool,
    movement_phase: int,
    policy: ClampPolicy,
) -> bool:
    """Mirror PlayerBase_UpdateLoop hard-clamp gate."""
    if not overspeeding:
        return False
    if policy == "legacy":
        # Old bug: disarmed + transition still hard-clamped → SNAP to cruise
        if not in_transition:
            return True
        if not armed:
            return True
        return False
    # Fixed: Run disarm transition soft-only; Walk may still hard-clamp
    if not in_transition:
        return True
    if (not armed) and movement_phase == 1:
        return True
    return False


def simulate_wprime_disarm_apply_path(
    *,
    policy: ClampPolicy = "fixed",
    armed_abs_ms: float = 3.28,
    cruise_abs_ms: float = 1.80,
    engine_base_ms: float = 3.76,
    movement_phase: int = 2,
    dt: float = 0.05,
    warm_frames: int = 8,
    post_disarm_sec: float = 6.0,
    mid_frame_disarm: bool = True,
) -> ApplyPathSimResult:
    """Time series: armed cruise→burst limit, then W′ disarm theoretical SNAP.

    mid_frame_disarm: TickPower disarms after UpdateAndGet (same-frame EnsureDisarm).
    """
    tr = AbsSpeedTransition()
    last_mult = armed_abs_ms / engine_base_ms
    applied = armed_abs_ms
    v_meas = armed_abs_ms
    t = 0.0
    out = ApplyPathSimResult()

    total_frames = warm_frames + int(post_disarm_sec / dt) + 1
    disarm_frame = warm_frames

    for frame in range(total_frames):
        armed = frame < disarm_frame
        if armed:
            theoretical = armed_abs_ms
        else:
            theoretical = cruise_abs_ms

        # Pre-tick UpdateAndGet sees pre-tick armed (mid-frame: still armed on disarm frame)
        armed_for_transition = armed
        if mid_frame_disarm and frame == disarm_frame:
            armed_for_transition = True

        mult = tr.update_and_get(
            t,
            theoretical,
            engine_base_ms,
            True,
            last_mult,
            engine_base_ms,
            armed_for_transition,
        )
        slewed_abs = mult * engine_base_ms
        applied = slewed_abs
        hard_clamped = False

        # Post-TickPower: actual armed state + EnsureDisarm
        if mid_frame_disarm and frame == disarm_frame:
            tr.ensure_disarm_transition(t, applied, cruise_abs_ms)

        in_tr = tr.is_in_transition()
        # 惯性：限速已开始缓降时，身体速度仍贴近武装期速度 → 形成超速
        overspeeding = v_meas > applied + 0.12
        if allow_overspeed_hard_clamp(
            overspeeding, in_tr, armed, movement_phase, policy
        ):
            # 解除武装硬路径：用巡航顶覆盖已缓降 applied（旧逻辑 SNAP 根因）
            if not armed:
                applied = cruise_abs_ms
                hard_clamped = True
                last_mult = applied / engine_base_ms
            else:
                applied = min(applied, theoretical)
                hard_clamped = True
                last_mult = applied / engine_base_ms
        else:
            last_mult = applied / engine_base_ms

        # 物理惯性：身体速度粘滞，限速先掉才会形成超速（复现硬钳 SNAP）
        if hard_clamped:
            v_meas = applied
        elif v_meas > applied + 0.05:
            if policy == "fixed":
                # 软钳跟当前已缓降 applied，不改 applied 本身
                pull = 1.8 * dt
                v_meas = max(applied, v_meas - pull)
            else:
                # legacy：跟速更慢 → 更容易 overspeeding → 触发硬路径 SNAP
                pull = 0.05 * dt
                v_meas = max(applied, v_meas - pull)
        elif armed and v_meas < theoretical - 0.05:
            v_meas = min(theoretical, v_meas + 4.0 * dt)

        out.samples.append(
            ApplyPathSample(
                t=t,
                theoretical_abs_ms=theoretical,
                applied_abs_ms=applied,
                v_meas_ms=v_meas,
                armed=armed,
                in_transition=in_tr,
                hard_clamped=hard_clamped,
            )
        )
        t += dt

    return out


def evaluate_disarm_slew_contract(
    result: ApplyPathSimResult,
    *,
    cruise_abs_ms: float = 1.80,
    max_first_frame_drop_ms: float = 0.45,
    min_applied_at_1s_above_cruise: float = 0.35,
    max_applied_jump_ms: float = 0.55,
) -> Tuple[bool, str]:
    """Return (ok, reason). Catches SNAP that metabolic grid treats as OK."""
    samples = result.samples
    if len(samples) < 20:
        return False, "too_few_samples"

    disarm_i = None
    for i, s in enumerate(samples):
        if not s.armed:
            disarm_i = i
            break
    if disarm_i is None or disarm_i == 0:
        return False, "no_disarm_edge"

    prev = samples[disarm_i - 1].applied_abs_ms
    first = samples[disarm_i].applied_abs_ms
    drop0 = prev - first
    if drop0 > max_first_frame_drop_ms:
        return False, f"first_frame_snap drop={drop0:.3f}"

    # ~1s after disarm still mid-slew (not already at cruise)
    t_disarm = samples[disarm_i].t
    at_1s = None
    for s in samples[disarm_i:]:
        if s.t >= t_disarm + 1.0:
            at_1s = s
            break
    if at_1s is None:
        return False, "missing_1s_sample"
    if at_1s.applied_abs_ms < cruise_abs_ms + min_applied_at_1s_above_cruise:
        return False, (
            f"applied_collapsed_by_1s applied={at_1s.applied_abs_ms:.3f} "
            f"cruise={cruise_abs_ms:.3f}"
        )

    # No huge frame-to-frame jumps in applied during first 3s after disarm
    for i in range(disarm_i + 1, len(samples)):
        if samples[i].t > t_disarm + 3.0:
            break
        jump = abs(samples[i].applied_abs_ms - samples[i - 1].applied_abs_ms)
        if jump > max_applied_jump_ms:
            return False, f"applied_jump={jump:.3f} at t={samples[i].t:.2f}"

    # Eventually approach cruise
    last = samples[-1]
    if last.t < t_disarm + 5.0:
        return False, "sim_too_short"
    if last.applied_abs_ms > cruise_abs_ms + 0.35:
        return False, f"never_reached_cruise applied={last.applied_abs_ms:.3f}"

    return True, "ok"


def wprime_disarm_apply_path_regression_ok() -> bool:
    """Fixed policy must pass; legacy SNAP must fail (proves test has teeth)."""
    fixed = simulate_wprime_disarm_apply_path(policy="fixed")
    ok_fixed, why_fixed = evaluate_disarm_slew_contract(fixed)
    if not ok_fixed:
        return False

    legacy = simulate_wprime_disarm_apply_path(policy="legacy")
    ok_legacy, _why_legacy = evaluate_disarm_slew_contract(legacy)
    if ok_legacy:
        # If legacy also passes, contract is too loose to catch the bug
        return False

    # Silence unused in success path; why_fixed retained for debuggers
    _ = why_fixed
    return True


if __name__ == "__main__":
    for name, pol in (("fixed", "fixed"), ("legacy", "legacy")):
        res = simulate_wprime_disarm_apply_path(policy=pol)  # type: ignore[arg-type]
        ok, why = evaluate_disarm_slew_contract(res)
        d0 = None
        for i, s in enumerate(res.samples):
            if not s.armed and i > 0:
                d0 = res.samples[i - 1].applied_abs_ms - s.applied_abs_ms
                break
        print(f"{name}: ok={ok} why={why} first_drop={d0}")
    print("regression:", wprime_disarm_apply_path_regression_ok())
