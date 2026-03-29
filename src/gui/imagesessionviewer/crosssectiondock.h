#ifndef CROSSSECTIONDOCK_H
#define CROSSSECTIONDOCK_H

#include <QDockWidget>
#include "datacrosssectionwidget.h"

class CrossSectionDock : public QDockWidget
{
	Q_OBJECT
public:
	explicit CrossSectionDock(QWidget* parent = nullptr)
		: QDockWidget("Cross-Section Viewer", parent)
	{
		this->setObjectName("CrossSectionDock");
		this->setAllowedAreas(Qt::AllDockWidgetAreas);
		this->setWidget(new DataCrossSectionWidget(this));
	}

	DataCrossSectionWidget* crossSectionWidget() const
	{
		return static_cast<DataCrossSectionWidget*>(this->widget());
	}
};

#endif // CROSSSECTIONDOCK_H
