#include "dialog.hpp"
#include "../pw_helper.hpp"

#include <QtWidgets/QApplication>

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setApplicationName("PipeWire ASIO Settings");

	PwHelper::InitArgs const config = {
		.app_name = "pw-asio settings",
	};
	PwHelper::Helper *helper = PwHelper::create_helper(argc, argv, &config);
	if (!helper) {
		std::fputs("Failed to connect to PipeWire\n", stderr);
		return 1;
	}

	PwAsioDialog dialog(helper);
	dialog.show();

	int res = app.exec();
	PwHelper::destroy_helper(helper);
	return res;
}
