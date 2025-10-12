#pragma once

#include <QDockWidget>
#include "messageconsolewidget.h"

/*
	Simple QDockWidget that hosts MessageConsoleWidget.
*/

class MessageConsoleDock : public QDockWidget
{
	Q_OBJECT
public:
	explicit MessageConsoleDock(QWidget* parent = nullptr)
		: QDockWidget("Message Console", parent)
	{
		this->setObjectName("MessageConsoleDock");
		this->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
		this->setWidget(new MessageConsoleWidget(this));
	}
};
