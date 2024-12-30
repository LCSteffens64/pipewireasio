all:

python: ui_dialog.py ui_device_selector.py

CFLAGS = -fpic -std=c++20 -ggdb
LFLAGS = -lQt5Core -lQt5Widgets -lQt5Gui -fpic -lpipewire-0.3

app: app.o dialog.o device_selector.o device_chooser.o pw_helper.o
	$(CXX) -o $@ $(LFLAGS) $^

device_selector.o: ui_device_selector.hpp
dialog.o: ui_dialog.hpp

QT_CFLAGS = -isystem /usr/include/qt
PW_CFLAGS = -isystem /usr/include/pipewire-0.3 -isystem /usr/include/spa-0.2

dialog.o: dialog.moc
device_selector.o: device_selector.moc
device_chooser.o: device_chooser.moc

%.o: %.cpp
	$(CXX) -c -o $@ $(CFLAGS) $(QT_CFLAGS) $(PW_CFLAGS) $<

pw_helper.o: ../pw_helper.cpp
	$(CXX) -c -o $@ $(CFLAGS) $(PW_CFLAGS) $<

%.moc: %.hpp
	moc -o $@ $<

ui_%.py: %.ui
	pyuic5 $< -o $@

ui_%.hpp: %.ui
	uic $< -o $@
