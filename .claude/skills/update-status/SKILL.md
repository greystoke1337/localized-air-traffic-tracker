---
name: update-status
description: Update the Foxtrot project status page (status.html) to reflect current progress. Use when the user says "update status", "update status page", or after completing Foxtrot-related work (flashing firmware, deploying server, testing features, validating on hardware).
argument-hint: "[milestone] [status]"
---

# Update Foxtrot Project Status Page

Update `status.html` to reflect the current state of the Foxtrot Chicago deployment.

## Context

The status page at `status.html` (deployed to `overheadtracker.com/status`) tracks the Foxtrot device project with a milestone timeline. Each milestone has three possible status tags:

- **IMPLEMENTED** (amber) — code written
- **TESTED** (blue) — automated or manual tests pass
- **VALIDATED** (green) — verified on real hardware by the developer

## Step 1: Assess what changed

Read `status.html` and look at the current milestone states. Then determine what has changed by reviewing:

1. **Recent conversation context** — what did we just do? (flash firmware, deploy server, run tests, etc.)
2. **Git status** — `git diff --name-only` to see modified files
3. **User's explicit instructions** — if they specified a milestone and status (e.g., "mark heartbeat as validated")

## Step 2: Determine which milestones to update

Map the work done to specific milestones in `status.html`. Common mappings:

| Action | Milestone(s) affected | Tag to add |
|--------|----------------------|------------|
| Flashed firmware to Foxtrot | "Device flash & end-to-end test" → done | TESTED |
| User confirmed device works | Relevant milestone | VALIDATED |
| Deployed server to Railway | "Server deployment" → done | TESTED |
| Ran server tests (`npm test`) | "Device heartbeat" or relevant | TESTED |
| Ran load test | "Proxy server" | TESTED |
| Verified heartbeat appears | "Device heartbeat" | VALIDATED |
| Tested flash tool with real device | "Firmware update tool" | TESTED |
| User verified on real hardware | Any milestone | VALIDATED |
| New feature implemented | Add new milestone or update existing | IMPLEMENTED |
| Shipped device | "Shipping" → done | VALIDATED |

## Step 3: Edit status.html

For each milestone that needs updating, edit the HTML in the `<div class="timeline">` section:

### To add a tag to an existing milestone

Find the milestone's `<div class="tl-tags">` block and add the appropriate tag span:

```html
<span class="tl-tag tested">TESTED</span>
<span class="tl-tag validated">VALIDATED</span>
```

Tag order must always be: IMPLEMENTED → TESTED → VALIDATED.

### To move a milestone from pending to done

Change `class="tl-item pending"` to `class="tl-item done"` and add the appropriate tags:

```html
<div class="tl-item done">
  <div class="tl-dot"></div>
  <div class="tl-label">Milestone name</div>
  <div class="tl-detail">Description of what was done</div>
  <div class="tl-tags">
    <span class="tl-tag implemented">IMPLEMENTED</span>
  </div>
</div>
```

### To add a new milestone

Insert a new `<div class="tl-item done">` block at the appropriate position in the timeline (before the pending items).

### To update detail text

Edit the `.tl-detail` content to reflect what was actually done or to add more specifics.

## Step 4: Report

Tell the user:
- Which milestones were updated
- What tags were added or changed
- Current overall progress (X of Y milestones complete, Z fully validated)
