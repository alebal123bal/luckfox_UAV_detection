## Command Output Policy

## When Asked About Detections

1. Run `read_detections`.
2. Compute centroid (cx = x + w/2, cy = y + h/2) for each detection.
3. Compute bbox area (w * h) for each detection.
4. Report ONLY the 5 bullets below. Nothing else.

## Output (exactly this, always)

- Trajectory: [left/right/up/down/stable, with pixel delta]
- Approach: [closing/receding/stable, based on bbox area trend]
- Speed: [fast/slow/hover, based on centroid delta per second]
- Duration: [first → last timestamp, total seconds]
- Pattern: [linear/erratic/hovering/crossing]
