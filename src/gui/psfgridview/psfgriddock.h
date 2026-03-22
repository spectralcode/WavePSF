#pragma once

#include <QDockWidget>
#include "psfgridwidget.h"

class PSFGridDock : public QDockWidget
{
	Q_OBJECT
public:
	explicit PSFGridDock(QWidget* parent = nullptr)
		: QDockWidget("PSF Grid", parent)
	{
		this->setObjectName("PSFGridDock");
		this->setAllowedAreas(Qt::AllDockWidgetAreas);
		this->setWidget(new PSFGridWidget(this));
	}
};
