# Settings System Usage Guide


## Overview

WavePSF stores all user-visible settings (anything the user can change in the GUI) in a single file: **`wavepsf.ini`**.

Only **`MainWindow`** talks to the settings backend (`SettingsFileManager`). Each widget provides:
- `QString getName() const;` — unique settings group
- `QVariantMap getSettings() const;` — current state
- `void setSettings(const QVariantMap&)` — apply immediately (UI + logic)
Settings are usually saved when the application is closed and restored on startup. 

**On startup:** `MainWindow` restores its own geometry/state, creates widgets/docks, then calls `setSettings(...)` on each widget with data from `wavepsf.ini`.

**On Shutdown (and key user actions):** `MainWindow` calls `getSettings()` on each widget and writes all settings back to `wavepsf.ini`.



## How to Add Settings to Your Class

### 1) Include Dependencies
	// MyWidget.cpp/.h
	#include <QVariantMap>	// widget returns/accepts a map only

### 2) Header: add the three methods getName, getSettings, setSettings
	// MyWidget.h
	#ifndef MYWIDGET_H
	#define MYWIDGET_H

	#include <QWidget>
	#include <QVariantMap>

	class MyWidget : public QWidget {
		Q_OBJECT
	public:
		explicit MyWidget(QWidget* parent = nullptr);

		// Settings round-trip + unique group name
		QString getName() const;
		QVariantMap getSettings() const;
		void setSettings(const QVariantMap& m);

	private:
		// Runtime defaults used if nothing is stored
		bool showGrid = false;
		int zoom = 100;

		// Apply current state to UI + business logic
		void applyState();
	};

	#endif // MYWIDGET_H

### 3) Define constants in the .cpp only (anonymous namespace)
	// MyWidget.cpp — constants local to this file
	namespace {
		const char* SETTINGS_GROUP = "my_widget";
		const char* SHOW_GRID_KEY  = "show_grid";
		const char* ZOOM_KEY       = "zoom";
	}

### 4) Implement getName / getSettings / setSettings (apply immediately)
	// MyWidget.cpp
	#include "MyWidget.h"

	MyWidget::MyWidget(QWidget* parent) : QWidget(parent) {
		// Use runtime defaults until MainWindow injects stored settings
		this->applyState();	// update UI + logic
	}

	QString MyWidget::getName() const {
		return QLatin1String(SETTINGS_GROUP);
	}

	QVariantMap MyWidget::getSettings() const {
		QVariantMap m;
		m.insert(QLatin1String(SHOW_GRID_KEY), this->showGrid);
		m.insert(QLatin1String(ZOOM_KEY), this->zoom);
		return m;
	}

	void MyWidget::setSettings(const QVariantMap& m) {
		// Inline defaults keep it robust
		const bool newShowGrid = m.value(QLatin1String(SHOW_GRID_KEY), false).toBool();
		const int  newZoom     = m.value(QLatin1String(ZOOM_KEY), 100).toInt();

		bool dirty = false;
		if (this->showGrid != newShowGrid) { this->showGrid = newShowGrid; dirty = true; }
		if (this->zoom != newZoom)         { this->zoom = newZoom;         dirty = true; }

		if (dirty) this->applyState();	// immediately update UI + business logic
	}

	void MyWidget::applyState() {
		// Update UI + logic from showGrid/zoom
		// e.g. this->plot->setGridVisible(this->showGrid);
		// e.g. this->view->setZoom(this->zoom);
	}

### 5) Update MainWindow load/save
    When adding a new widget loadSettings and saveSettings need to be updated to include the setSettings and getSettings methods from the new widget. This might change in the future. 
