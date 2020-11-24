# -*- coding: utf-8 -*-

"""
@file		ui_ezstreamer.py
@date		25 Nov 2020
@brief		UI file for ezstreamer.py
@see		https://github.com/nnstreamer/nnstreamer-example
@author		Jongha Jang <jangjongha.sw@gmail.com>
@bug		No Known bugs
This code is used for the purpose of putting the UI settings of ezstreamer.py. See ezstreamer.py for more information.
"""

################################################################################
## Form generated from reading UI file 'ezstreamer.ui'
##
## Created by: Qt User Interface Compiler version 5.14.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide2.QtCore import (QCoreApplication, QDate, QDateTime, QMetaObject,
    QObject, QPoint, QRect, QSize, QTime, QUrl, Qt)
from PySide2.QtGui import (QBrush, QColor, QConicalGradient, QCursor, QFont,
    QFontDatabase, QIcon, QKeySequence, QLinearGradient, QPalette, QPainter,
    QPixmap, QRadialGradient)
from PySide2.QtWidgets import *


class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(309, 479)
        font = QFont()
        font.setStyleStrategy(QFont.PreferDefault)
        MainWindow.setFont(font)
        MainWindow.setCursor(QCursor(Qt.ArrowCursor))
        MainWindow.setDocumentMode(False)
        self.centralwidget = QWidget(MainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.verticalLayout = QVBoxLayout(self.centralwidget)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout.setContentsMargins(-1, 9, -1, -1)
        self.main_vertical = QVBoxLayout()
        self.main_vertical.setObjectName(u"main_vertical")
        self.mode_label = QLabel(self.centralwidget)
        self.mode_label.setObjectName(u"mode_label")
        font1 = QFont()
        font1.setPointSize(15)
        font1.setBold(True)
        font1.setWeight(75)
        self.mode_label.setFont(font1)

        self.main_vertical.addWidget(self.mode_label)

        self.mode_layout = QHBoxLayout()
        self.mode_layout.setObjectName(u"mode_layout")
        self.nothing_radio = QRadioButton(self.centralwidget)
        self.nothing_radio.setObjectName(u"nothing_radio")
        self.nothing_radio.setChecked(True)

        self.mode_layout.addWidget(self.nothing_radio)

        self.fm_radio = QRadioButton(self.centralwidget)
        self.fm_radio.setObjectName(u"fm_radio")

        self.mode_layout.addWidget(self.fm_radio)

        self.em_radio = QRadioButton(self.centralwidget)
        self.em_radio.setObjectName(u"em_radio")

        self.mode_layout.addWidget(self.em_radio)


        self.main_vertical.addLayout(self.mode_layout)

        self.od_check = QCheckBox(self.centralwidget)
        self.od_check.setObjectName(u"od_check")

        self.main_vertical.addWidget(self.od_check)

        self.option_label = QLabel(self.centralwidget)
        self.option_label.setObjectName(u"option_label")
        self.option_label.setFont(font1)

        self.main_vertical.addWidget(self.option_label)

        self.lo_check = QCheckBox(self.centralwidget)
        self.lo_check.setObjectName(u"lo_check")
        self.lo_check.setChecked(True)

        self.main_vertical.addWidget(self.lo_check)

        self.rtmp_check = QCheckBox(self.centralwidget)
        self.rtmp_check.setObjectName(u"rtmp_check")

        self.main_vertical.addWidget(self.rtmp_check)

        self.dm_check = QCheckBox(self.centralwidget)
        self.dm_check.setObjectName(u"dm_check")

        self.main_vertical.addWidget(self.dm_check)

        self.roi_check = QCheckBox(self.centralwidget)
        self.roi_check.setObjectName(u"roi_check")

        self.main_vertical.addWidget(self.roi_check)

        self.label = QLabel(self.centralwidget)
        self.label.setObjectName(u"label")
        font2 = QFont()
        font2.setPointSize(13)
        font2.setBold(True)
        font2.setWeight(75)
        self.label.setFont(font2)

        self.main_vertical.addWidget(self.label)

        self.masking_layout = QHBoxLayout()
        self.masking_layout.setObjectName(u"masking_layout")
        self.knife_check = QCheckBox(self.centralwidget)
        self.knife_check.setObjectName(u"knife_check")
        self.knife_check.setEnabled(False)

        self.masking_layout.addWidget(self.knife_check)

        self.scissor_check = QCheckBox(self.centralwidget)
        self.scissor_check.setObjectName(u"scissor_check")
        self.scissor_check.setEnabled(False)

        self.masking_layout.addWidget(self.scissor_check)


        self.main_vertical.addLayout(self.masking_layout)

        self.auth_label = QLabel(self.centralwidget)
        self.auth_label.setObjectName(u"auth_label")
        self.auth_label.setEnabled(True)
        font3 = QFont()
        font3.setPointSize(15)
        self.auth_label.setFont(font3)

        self.main_vertical.addWidget(self.auth_label)

        self.auth_input = QLineEdit(self.centralwidget)
        self.auth_input.setObjectName(u"auth_input")
        self.auth_input.setEnabled(False)
        self.auth_input.setClearButtonEnabled(False)

        self.main_vertical.addWidget(self.auth_input)

        self.start_stop_button = QPushButton(self.centralwidget)
        self.start_stop_button.setObjectName(u"start_stop_button")
        font4 = QFont()
        font4.setPointSize(20)
        self.start_stop_button.setFont(font4)

        self.main_vertical.addWidget(self.start_stop_button)

        self.exit_button = QPushButton(self.centralwidget)
        self.exit_button.setObjectName(u"exit_button")
        self.exit_button.setFont(font4)

        self.main_vertical.addWidget(self.exit_button)


        self.verticalLayout.addLayout(self.main_vertical)

        MainWindow.setCentralWidget(self.centralwidget)
        self.statusbar = QStatusBar(MainWindow)
        self.statusbar.setObjectName(u"statusbar")
        MainWindow.setStatusBar(self.statusbar)

        self.retranslateUi(MainWindow)

        QMetaObject.connectSlotsByName(MainWindow)
    # setupUi

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QCoreApplication.translate("MainWindow", u"NNStreamer Example - EZStreamer", None))
        self.mode_label.setText(QCoreApplication.translate("MainWindow", u"MODE", None))
        self.nothing_radio.setText(QCoreApplication.translate("MainWindow", u"NOTHING", None))
        self.fm_radio.setText(QCoreApplication.translate("MainWindow", u"Face Mask", None))
        self.em_radio.setText(QCoreApplication.translate("MainWindow", u"Eye Mask", None))
        self.od_check.setText(QCoreApplication.translate("MainWindow", u"Object Detection", None))
        self.option_label.setText(QCoreApplication.translate("MainWindow", u"OPTION", None))
        self.lo_check.setText(QCoreApplication.translate("MainWindow", u"Local Output", None))
        self.rtmp_check.setText(QCoreApplication.translate("MainWindow", u"RTMP (Youtube)", None))
        self.dm_check.setText(QCoreApplication.translate("MainWindow", u"Detect Main Streamer", None))
        self.roi_check.setText(QCoreApplication.translate("MainWindow", u"Draw Region Of Interests", None))
        self.label.setText(QCoreApplication.translate("MainWindow", u"MASKING OBJECTS", None))
        self.knife_check.setText(QCoreApplication.translate("MainWindow", u"Knife", None))
        self.scissor_check.setText(QCoreApplication.translate("MainWindow", u"Scissors", None))
        self.auth_label.setText(QCoreApplication.translate("MainWindow", u"YOUTUBE RTMP AUTH KEY", None))
        self.auth_input.setPlaceholderText(QCoreApplication.translate("MainWindow", u"Input your Youtube auth key", None))
        self.start_stop_button.setText(QCoreApplication.translate("MainWindow", u"START", None))
        self.exit_button.setText(QCoreApplication.translate("MainWindow", u"EXIT", None))
    # retranslateUi

