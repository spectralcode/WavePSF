# Settings System Usage Guide


## Overview

WavePSF stores all user-visible settings in a single human-readable file: **`wavepsf.ini`**.

Only **`MainWindow`** talks to the settings backend (`SettingsFileManager`). Each widget provides:
- `QString getName() const;` — unique settings group name (used as INI section)
- `QVariantMap getSettings() const;` — current state as a flat or nested map
- `void setSettings(const QVariantMap&)` — apply immediately (UI + logic)

**On startup:** `MainWindow` restores its own geometry/state, then calls `setSettings(...)` on each widget with data loaded from `wavepsf.ini`.

**On shutdown (and key user actions):** `MainWindow` calls `getSettings()` on each widget and writes all settings back to `wavepsf.ini`.

Nested `QVariantMap` values are stored recursively as `[group/subgroup]` INI sections — not binary blobs — so the file is always human-readable and hand-editable.


## How to Add Settings to Your Class

### 1) Header: declare the three methods

```cpp
// MyWidget.h
#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>
#include <QVariantMap>

class MyWidget : public QWidget {
    Q_OBJECT
public:
    explicit MyWidget(QWidget* parent = nullptr);

    QString getName() const;
    QVariantMap getSettings() const;
    void setSettings(const QVariantMap& m);
};

#endif // MYWIDGET_H
```

### 2) Define KEY and DEF constants in the .cpp (anonymous namespace only)

```cpp
// MyWidget.cpp
namespace {
    const QString SETTINGS_GROUP = QStringLiteral("my_widget");

    // Key names — lower_snake_case, stored verbatim as INI keys
    const QString KEY_SHOW_GRID = QStringLiteral("show_grid");
    const QString KEY_ZOOM      = QStringLiteral("zoom");

    // Default values — used when the key is absent (fresh install, corrupt file)
    const bool DEF_SHOW_GRID = false;
    const int  DEF_ZOOM      = 100;
}
```

Rules:
- Always `const QString` + `QStringLiteral(...)`, never `const char*`
- Key constant names: `KEY_` prefix, `UPPER_SNAKE_CASE`
- Default constant names: `DEF_` prefix, `UPPER_SNAKE_CASE`
- Key string values: `lower_snake_case`
- Keep all constants in the `.cpp` anonymous namespace — never expose them in the header

### 3) Implement getName / getSettings / setSettings

```cpp
QString MyWidget::getName() const {
    return SETTINGS_GROUP;
}

QVariantMap MyWidget::getSettings() const {
    QVariantMap m;
    m.insert(KEY_SHOW_GRID, this->showGridAction->isChecked());
    m.insert(KEY_ZOOM,      this->zoom);
    return m;
}

void MyWidget::setSettings(const QVariantMap& m) {
    this->showGridAction->setChecked(m.value(KEY_SHOW_GRID, DEF_SHOW_GRID).toBool());
    this->zoom = m.value(KEY_ZOOM, DEF_ZOOM).toInt();
    // Apply to UI/logic immediately...
}
```

### 4) Nested maps (for child widgets or sub-settings)

If your widget owns child widgets that also have settings, nest them as sub-maps:

```cpp
namespace {
    const QString SETTINGS_GROUP = QStringLiteral("my_widget");
    const QString KEY_CHILD_PLOT = QStringLiteral("plot");
    const QString KEY_SHOW_GRID  = QStringLiteral("show_grid");
    const bool    DEF_SHOW_GRID  = true;
}

QVariantMap MyWidget::getSettings() const {
    QVariantMap m;
    m.insert(KEY_SHOW_GRID,  this->showGrid);
    m.insert(KEY_CHILD_PLOT, this->plotWidget->getSettings());  // nested map
    return m;
}

void MyWidget::setSettings(const QVariantMap& m) {
    this->showGrid = m.value(KEY_SHOW_GRID, DEF_SHOW_GRID).toBool();
    this->plotWidget->setSettings(m.value(KEY_CHILD_PLOT).toMap());
}
```

This produces readable INI sections:
```ini
[my_widget]
show_grid=true

[my_widget/plot]
colormap_index=13
auto_scale=true
```

### 5) Register in MainWindow

Add `getSettings()`/`setSettings()` calls for the new widget in `MainWindow::saveSettings()` and `MainWindow::loadSettings()`.
