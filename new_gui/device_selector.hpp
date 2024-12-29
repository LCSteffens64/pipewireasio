#pragma once

#include <QtWidgets/QGroupBox>
#include <QtWidgets/QWidget>

#include "ui_device_selector.hpp"

enum class PwIODeviceKind {
	Input,
	Output,
};

class PwIODeviceSelector: public QGroupBox {
	Q_OBJECT

	public:
	PwIODeviceSelector(QWidget *parent);

	PwIODeviceKind deviceKind = PwIODeviceKind::Input;

	private:
	Ui::PwIODeviceSelector ui;
};
