#pragma once

// ── Grid constants ──────────────────────────────────────────────────────────
constexpr int kGridMarginX      = 8;
constexpr int kGridMarginY      = 8;
constexpr int kIconExtraWidth   = 32;   // extra width beyond iconSize (padding + text area)
constexpr int kIconExtraHeight  = 48;   // extra height beyond iconSize (icon + label)

// ── Drag-and-drop ───────────────────────────────────────────────────────────
constexpr double kDragGhostOpacity = 0.55;

// ── Tooltip ─────────────────────────────────────────────────────────────────
constexpr int kTooltipOffsetY    = 18;   // pixels below cursor
constexpr int kTooltipShowDelay  = 700;  // ms before tooltip appears
constexpr int kTooltipScreenMargin = 2;  // min distance from screen edge

// ── Icon rendering ──────────────────────────────────────────────────────────
constexpr int kIconLabelMarginX  = 2;    // left/right margin inside label rect
constexpr int kIconLabelMarginTop = 3;   // gap between icon image and label
constexpr int kIconLabelExtraLines = 1;  // extra lines when selected (3 vs 2)
constexpr int kIconLabelMinHeightPadding = 4; // extra px around text highlight
constexpr int kIconLabelPadding = 4;   // horizontal padding inside label area
constexpr int kIconPreviewScale = 96;   // max thumbnail preload size

// ── Icon label elision ─────────────────────────────────────────────────────
constexpr int kElisionCandidateStep = 8;  // mid-word fallback every N chars

// ── Config defaults ────────────────────────────────────────────────────────
constexpr int kDefaultIconSize   = 40;
constexpr int kDefaultFontSize   = 8;
constexpr int kRefreshDebounceMs = 400;  // debounced refresh after dir change
constexpr int kSaveDebounceMs    = 20;   // debounced save after drag/rename
constexpr int kCreateShortcutRefreshMs = 10;

// ── File watching ──────────────────────────────────────────────────────────
constexpr int kDirChangeDebounceMs = 400;

// ── IPC buffer ─────────────────────────────────────────────────────────────
constexpr int kMaxIPCBufferSize = 64 * 1024;  // 64 KB

// ── Dialog sizes ───────────────────────────────────────────────────────────
constexpr int kPropertiesDialogWidth  = 360;
constexpr int kPropertiesDialogBannerHeight = 52;
constexpr int kPropertiesDialogContentMargins = 8;

// ── Slider ranges ──────────────────────────────────────────────────────────
constexpr int kIconSizeSliderMin = 24;
constexpr int kIconSizeSliderMax = 96;
constexpr int kIconSizeSliderSingleStep = 4;
constexpr int kIconSizeSliderPageStep   = 8;
constexpr int kIconSizeSliderTickInterval = 8;

constexpr int kFontSizeSpinMin = 6;
constexpr int kFontSizeSpinMax = 24;

// ── Hyprland reserved-area query timeout ───────────────────────────────────
constexpr int kHyprctlMonitorsTimeoutMs = 1500;

// ── Hyprland exec dispatch timeout ─────────────────────────────────────────
constexpr int kHyprctlDispatchTimeoutMs = 1500;

// ── Mime query timeout ─────────────────────────────────────────────────────
constexpr int kMimeQueryTimeoutMs = 1000;

// ── File size thresholds (human-readable) ──────────────────────────────────
constexpr qint64 kKB = 1024LL;
constexpr qint64 kMB = 1024LL * 1024;
constexpr qint64 kGB = 1024LL * 1024 * 1024;
