/*
 * Stellarium
 * Copyright (C) 2008 Fabien Chereau
 * Copyright (C) 2012 Timothy Reaves
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "SkyGui.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelMovementMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelMainView.hpp"
#include "StelObjectMgr.hpp"
#include "StelActionMgr.hpp"
#include "StelPropertyMgr.hpp"
#include "StelFileMgr.hpp"

#include "StelObjectType.hpp"
#include "StelObject.hpp"
#include "StelStyle.hpp"
#ifdef ENABLE_SCRIPT_CONSOLE
#include "ScriptConsole.hpp"
#endif
#ifdef ENABLE_SCRIPTING
#include "StelScriptMgr.hpp"
#endif

#include "ConfigurationDialog.hpp"
#include "DateTimeDialog.hpp"
#include "HelpDialog.hpp"
#include "LocationDialog.hpp"
#include "SearchDialog.hpp"
#include "ViewDialog.hpp"
#include "ShortcutsDialog.hpp"
#include "AstroCalcDialog.hpp"
#include "ObsListDialog.hpp"

#include <QDebug>
#include <QTimeLine>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QFile>
#include <QTextBrowser>
#include <QGraphicsWidget>
#include <QGraphicsGridLayout>
#include <QClipboard>
#include <QMimeData>
#include <QPalette>
#include <QColor>
#include <QAction>
#include <QKeySequence>

static QString applyScaleToCSS(const QString& css, const double scale)
{
	auto out = css;
	static const QRegularExpression pat("\\b([0-9.]+)px\\b");
	QRegularExpressionMatch match;
	int pos = 0;
	while((pos = out.indexOf(pat, pos, &match)) >= 0)
	{
		const auto numStr = match.captured(1);
		bool ok = false;
		const auto num = numStr.toDouble(&ok);
		assert(ok);
		const auto scaled = num * scale;
		const bool hasDot = numStr.contains(QLatin1Char('.'));
		const auto newNumStr = hasDot ? QString::number(scaled,'f',3)
		                              : QString::number(int(scaled));
		out.replace(pos, numStr.size(), newNumStr);
		pos += newNumStr.size() + 2; // skip the whole resulting pattern
	}
	return out;
}

StelGui::StelGui()
	: topLevelGraphicsWidget(nullptr)
	, skyGui(nullptr)
	, flagUseButtonsBackground(true)
	, flagUseKineticScrolling(false)
	, flagEnableFocusOnDaySpinner(false)
	, buttonTimeRewind(nullptr)
	, buttonTimeRealTimeSpeed(nullptr)
	, buttonTimeCurrent(nullptr)
	, buttonTimeForward(nullptr)
	, flagShowQuitButton(true)
	, buttonQuit(nullptr)
	, flagShowGotoSelectedObjectButton(true)
	, buttonGotoSelectedObject(nullptr)
	, locationDialog(nullptr)
	, helpDialog(nullptr)
	, dateTimeDialog(nullptr)
	, searchDialog(nullptr)
	, viewDialog(nullptr)
	, shortcutsDialog(nullptr)
	, configurationDialog(nullptr)
#ifdef ENABLE_SCRIPT_CONSOLE
	, scriptConsole(nullptr)
#endif
	, astroCalcDialog(nullptr)
	, obsListDialog(nullptr)
	, flagShowFlipButtons(false)
	, flipVert(nullptr)
	, flipHoriz(nullptr)
	, flagShowNebulaBackgroundButton(false)
	, btShowNebulaeBackground(nullptr)
	, flagShowDSSButton(false)
	, btShowDSS(nullptr)
	, flagShowHiPSButton(false)
	, btShowHiPS(nullptr)
	, flagShowNightmodeButton(true)
	, buttonNightmode(nullptr)
	, flagShowFullscreenButton(true)
	, buttonFullscreen(nullptr)
	, flagShowObsListButton(false)
	, btShowObsList(nullptr)
	, flagShowICRSGridButton(false)
	, btShowICRSGrid(nullptr)
	, flagShowGalacticGridButton(false)
	, btShowGalacticGrid(nullptr)
	, flagShowEclipticGridButton(false)
	, btShowEclipticGrid(nullptr)
	, flagShowConstellationBoundariesButton(false)
	, btShowConstellationBoundaries(nullptr)
	, flagShowConstellationArtsButton(false)
	, btShowConstellationArts(nullptr)
	, flagShowAsterismLinesButton(false)
	, btShowAsterismLines(nullptr)
	, flagShowAsterismLabelsButton(false)
	, btShowAsterismLabels(nullptr)
	, flagShowCardinalButton(false)
	, btShowCardinal(nullptr)
	, flagShowCompassButton(false)
	, btShowCompass(nullptr)
	, initDone(false)
#ifdef ENABLE_SCRIPTING
	  // We use a QStringList to save the user-configured buttons while script is running, and restore them later.
	, scriptSaveSpeedbuttons()
  #endif

{
	setObjectName("StelGui");
	// QPixmapCache::setCacheLimit(30000); ?
}

StelGui::~StelGui()
{
	delete skyGui;
	skyGui = nullptr;

	if (locationDialog)
	{
		delete locationDialog;
		locationDialog = nullptr;
	}
	if (helpDialog)
	{
		delete helpDialog;
		helpDialog = nullptr;
	}
	if (dateTimeDialog)
	{
		delete dateTimeDialog;
		dateTimeDialog = nullptr;
	}
	if (searchDialog)
	{
		delete searchDialog;
		searchDialog = nullptr;
	}
	if (viewDialog)
	{
		delete viewDialog;
		viewDialog = nullptr;
	}
	if (shortcutsDialog)
	{
		delete shortcutsDialog;
		shortcutsDialog = nullptr;
	}
	// configurationDialog is automatically deleted with its parent widget.
#ifdef ENABLE_SCRIPT_CONSOLE
	if (scriptConsole)
	{
		delete scriptConsole;
		scriptConsole = nullptr;
	}
#endif
	if (astroCalcDialog)
	{
		delete astroCalcDialog;
		astroCalcDialog = nullptr;
	}
	if (obsListDialog)
	{
		delete obsListDialog;
		obsListDialog = nullptr;
	}
}

void StelGui::updateStelStyle()
{
	setStelStyle(StelApp::getInstance().getCurrentStelStyle());
}

void StelGui::init(QGraphicsWidget *atopLevelGraphicsWidget)
{
	qInfo() << "Creating GUI ...";

	StelGuiBase::init(atopLevelGraphicsWidget);
	skyGui = new SkyGui(atopLevelGraphicsWidget);
	locationDialog = new LocationDialog(atopLevelGraphicsWidget);
	helpDialog = new HelpDialog(atopLevelGraphicsWidget);
	dateTimeDialog = new DateTimeDialog(atopLevelGraphicsWidget);
	searchDialog = new SearchDialog(atopLevelGraphicsWidget);
	viewDialog = new ViewDialog(atopLevelGraphicsWidget);
	shortcutsDialog = new ShortcutsDialog(atopLevelGraphicsWidget);
	configurationDialog = new ConfigurationDialog(this, atopLevelGraphicsWidget);
#ifdef ENABLE_SCRIPT_CONSOLE
	scriptConsole = new ScriptConsole(atopLevelGraphicsWidget);
#endif
	astroCalcDialog = new AstroCalcDialog(atopLevelGraphicsWidget);
	obsListDialog = new ObsListDialog(atopLevelGraphicsWidget);

	///////////////////////////////////////////////////////////////////////
	// Create all the main actions of the program, associated with shortcuts

	///////////////////////////////////////////////////////////////////////
	// Connect all the GUI actions signals with the Core of Stellarium
	StelActionMgr* actionsMgr = StelApp::getInstance().getStelActionManager();

	// XXX: this should probably go into the script manager.
	QString windowsGroup = N_("Windows");
	QString miscGroup = N_("Miscellaneous");
	QString infoGroup = N_("Selected object information");
	actionsMgr->addAction("actionQuit_Global", miscGroup, N_("Quit"), this, "quit()", "Ctrl+Q", "Ctrl+X");

#ifdef ENABLE_SCRIPTING
	QString datetimeGroup = N_("Date and Time");
	actionsMgr->addAction("actionIncrease_Script_Speed", datetimeGroup, N_("Speed up the script execution rate"), this, "increaseScriptSpeed()");
	actionsMgr->addAction("actionDecrease_Script_Speed", datetimeGroup, N_("Slow down the script execution rate"), this, "decreaseScriptSpeed()");
	actionsMgr->addAction("actionSet_Real_Script_Speed", datetimeGroup, N_("Set the normal script execution rate"), this, "setRealScriptSpeed()");
	actionsMgr->addAction("actionStop_Script", datetimeGroup, N_("Stop script execution"), this, "stopScript()", "Ctrl+D, S");
	#ifndef ENABLE_SCRIPT_QML
	actionsMgr->addAction("actionPause_Script", datetimeGroup, N_("Pause script execution"), this, "pauseScript()", "Ctrl+D, P");
	actionsMgr->addAction("actionResume_Script", datetimeGroup, N_("Resume script execution"), this, "resumeScript()", "Ctrl+D, R");
	#endif
#endif
#ifdef ENABLE_SCRIPT_CONSOLE
	actionsMgr->addAction("actionShow_ScriptConsole_Window_Global", windowsGroup, N_("Script console window"), scriptConsole, "visible", "F12", "", true);
#endif

	actionsMgr->addAction("actionShow_Help_Window_Global", windowsGroup, N_("Help window"), helpDialog, "visible", "F1", "", true);
	actionsMgr->addAction("actionShow_Configuration_Window_Global", windowsGroup, N_("Configuration window"), configurationDialog, "visible", "F2", "", true);
	actionsMgr->addAction("actionShow_Search_Window_Global", windowsGroup, N_("Search window"), searchDialog, "visible", "F3", "Ctrl+F", true);
	actionsMgr->addAction("actionShow_SkyView_Window_Global", windowsGroup, N_("Sky and viewing options window"), viewDialog, "visible", "F4", "", true);
	actionsMgr->addAction("actionShow_DateTime_Window_Global", windowsGroup, N_("Date/time window"), dateTimeDialog, "visible", "F5", "", true);
	actionsMgr->addAction("actionShow_Location_Window_Global", windowsGroup, N_("Location window"), locationDialog, "visible", "F6", "", true);
	actionsMgr->addAction("actionShow_Shortcuts_Window_Global", windowsGroup, N_("Shortcuts window"), shortcutsDialog, "visible", "F7", "", true);
	actionsMgr->addAction("actionShow_AstroCalc_Window_Global", windowsGroup, N_("Astronomical calculations window"), astroCalcDialog, "visible", "F10", "", true);
	actionsMgr->addAction("actionShow_ObsList_Window_Global", windowsGroup, N_("Observing list"), obsListDialog, "visible", "Alt+B", "", true);
	actionsMgr->addAction("actionSave_Copy_Object_Information_Global", miscGroup, N_("Copy selected object information to clipboard"), this, "copySelectedObjectInfo()", "Ctrl+Shift+C", "", true);

	QSettings* conf = StelApp::getInstance().getSettings();
	Q_ASSERT(conf);
	setAutoHideHorizontalButtonBar(conf->value("gui/auto_hide_horizontal_toolbar", true).toBool());
	setAutoHideVerticalButtonBar(conf->value("gui/auto_hide_vertical_toolbar", true).toBool());
	actionsMgr->addAction("actionAutoHideHorizontalButtonBar", miscGroup, N_("Auto hide horizontal button bar"), this, "autoHideHorizontalButtonBar");
	actionsMgr->addAction("actionAutoHideVerticalButtonBar", miscGroup, N_("Auto hide vertical button bar"), this, "autoHideVerticalButtonBar");

	setGuiVisible(conf->value("gui/flag_show_gui", true).toBool());
	actionsMgr->addAction("actionToggle_GuiHidden_Global", miscGroup, N_("Toggle visibility of GUI"), this, "visible", "Ctrl+T", "", true);

#ifdef ENABLE_SCRIPTING
	StelScriptMgr* scriptMgr = &StelApp::getInstance().getScriptMgr();
	connect(scriptMgr, SIGNAL(scriptRunning()), this, SLOT(scriptStarted()));
	connect(scriptMgr, SIGNAL(scriptStopped()), this, SLOT(scriptStopped()));
#endif

	actionsMgr->addAction("actionDisplayInfo_All",     infoGroup, N_("All available info"), this, "displayAllInfo()");
	actionsMgr->addAction("actionDisplayInfo_Default", infoGroup, N_("Default info"), this, "displayDefaultInfo()");
	actionsMgr->addAction("actionDisplayInfo_Short",   infoGroup, N_("Short info"), this, "displayShortInfo()");
	actionsMgr->addAction("actionDisplayInfo_None",    infoGroup, N_("None info"), this, "displayNoneInfo()");
	actionsMgr->addAction("actionDisplayInfo_Custom",  infoGroup, N_("Custom info"), this, "displayCustomInfo()");

	// Put StelGui under the StelProperty system (simpler and more consistent GUI)
	StelApp::getInstance().getStelPropertyManager()->registerObject(this);

	///////////////////////////////////////////////////////////////////////////
	//// QGraphicsView based GUI
	///////////////////////////////////////////////////////////////////////////

	setFlagUseButtonsBackground(conf->value("gui/flag_show_buttons_background", true).toBool());
	// Add everything
	QPixmap pxmapDefault, pxmapOn, pxmapOff;
	QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");

	// Add buttons on left bar (Window)
	addButtonOnLeftBar("bbtLocation", "actionShow_Location_Window_Global");
	addButtonOnLeftBar("bbtTime", "actionShow_DateTime_Window_Global");
	addButtonOnLeftBar("bbtSky", "actionShow_SkyView_Window_Global");
	addButtonOnLeftBar("bbtSearch", "actionShow_Search_Window_Global");
	addButtonOnLeftBar("bbtSettings", "actionShow_Configuration_Window_Global");
	addButtonOnLeftBar("bbtAstroCalc", "actionShow_AstroCalc_Window_Global");
	addButtonOnLeftBar("bbtHelp", "actionShow_Help_Window_Global");

	// Add buttons on bottom bar
	// Buttons for manage constellations
	QString groupName = "010-constellationsGroup";
	addButtonOnBottomBar("btConstellationLines", "actionShow_Constellation_Lines", groupName);
	addButtonOnBottomBar("btConstellationLabels", "actionShow_Constellation_Labels", groupName);
	// Buttons for manage grids
	groupName = "020-gridsGroup";
	addButtonOnBottomBar("btEquatorialGrid", "actionShow_Equatorial_Grid", groupName);
	addButtonOnBottomBar("btAzimuthalGrid", "actionShow_Azimuthal_Grid", groupName);
	// Buttons for manage landscapes
	groupName = "030-landscapeGroup";
	addButtonOnBottomBar("btGround", "actionShow_Ground", groupName);	
	addButtonOnBottomBar("btAtmosphere", "actionShow_Atmosphere", groupName);
	// Buttons for manage sky objects
	groupName = "040-nebulaeGroup";
	addButtonOnBottomBar("btNebula", "actionShow_Nebulas", groupName);
	addButtonOnBottomBar("btPlanets", "actionShow_Planets_Labels", groupName);
	// Buttons for manage other stuff
	groupName = "060-othersGroup";
	addButtonOnBottomBar("btEquatorialMount", "actionSwitch_Equatorial_Mount", groupName);

	// A "special" buttons
	pxmapOn = QPixmap(":/graphicGui/btGotoSelectedObject-on.png");
	pxmapOff = QPixmap(":/graphicGui/btGotoSelectedObject-off.png");
	buttonGotoSelectedObject = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionGoto_Selected_Object");
	skyGui->bottomBar->addButton(buttonGotoSelectedObject, "060-othersGroup");

	pxmapOn = QPixmap(":/graphicGui/btNightView-on.png");
	pxmapOff = QPixmap(":/graphicGui/btNightView-off.png");
	buttonNightmode = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Night_Mode");
	skyGui->bottomBar->addButton(buttonNightmode, "060-othersGroup");

	pxmapOn = QPixmap(":/graphicGui/btFullScreen-on.png");
	pxmapOff = QPixmap(":/graphicGui/btFullScreen-off.png");
	buttonFullscreen = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionSet_Full_Screen_Global");
	buttonFullscreen->setTriggerOnRelease(true);
	skyGui->bottomBar->addButton(buttonFullscreen, "060-othersGroup");

	pxmapOn = QPixmap(":/graphicGui/btTimeRewind-on.png");
	pxmapOff = QPixmap(":/graphicGui/btTimeRewind-off.png");
	buttonTimeRewind = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionDecrease_Time_Speed");
	skyGui->bottomBar->addButton(buttonTimeRewind, "070-timeGroup");

	pxmapOn = QPixmap(":/graphicGui/btTimeRealtime-on.png");
	pxmapOff = QPixmap(":/graphicGui/btTimeRealtime-off.png");
	pxmapDefault = QPixmap(":/graphicGui/btTimePause-on.png");
	buttonTimeRealTimeSpeed = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapDefault, pxmapGlow32x32, "actionSet_Real_Time_Speed");
	skyGui->bottomBar->addButton(buttonTimeRealTimeSpeed, "070-timeGroup");

	pxmapOn = QPixmap(":/graphicGui/btTimeNow-on.png");
	pxmapOff = QPixmap(":/graphicGui/btTimeNow-off.png");
	buttonTimeCurrent = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionReturn_To_Current_Time");
	skyGui->bottomBar->addButton(buttonTimeCurrent, "070-timeGroup");

	pxmapOn = QPixmap(":/graphicGui/btTimeForward-on.png");
	pxmapOff = QPixmap(":/graphicGui/btTimeForward-off.png");
	buttonTimeForward = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionIncrease_Time_Speed");
	skyGui->bottomBar->addButton(buttonTimeForward, "070-timeGroup");

	pxmapOn = QPixmap(":/graphicGui/btQuit.png");	
	buttonQuit = new StelButton(nullptr, pxmapOn, pxmapOn, pxmapGlow32x32, "actionQuit_Global");
	skyGui->bottomBar->addButton(buttonQuit, "080-quitGroup");

	// add the flip and other buttons if requested in the config
	setFlagShowFlipButtons(conf->value("gui/flag_show_flip_buttons", false).toBool());
	setFlagShowNebulaBackgroundButton(conf->value("gui/flag_show_nebulae_background_button", false).toBool());
	setFlagShowDSSButton(conf->value("gui/flag_show_dss_button", false).toBool());
	setFlagShowHiPSButton(conf->value("gui/flag_show_hips_button", false).toBool());
	setFlagShowGotoSelectedObjectButton(conf->value("gui/flag_show_goto_selected_button", true).toBool());
	setFlagShowNightmodeButton(conf->value("gui/flag_show_nightmode_button", true).toBool());
	setFlagShowFullscreenButton(conf->value("gui/flag_show_fullscreen_button", true).toBool());
	setFlagShowObsListButton(conf->value("gui/flag_show_obslist_button", false).toBool());
	setFlagShowICRSGridButton(conf->value("gui/flag_show_icrs_grid_button", false).toBool());
	setFlagShowGalacticGridButton(conf->value("gui/flag_show_galactic_grid_button", false).toBool());
	setFlagShowEclipticGridButton(conf->value("gui/flag_show_ecliptic_grid_button", false).toBool());
	setFlagShowConstellationArtsButton(conf->value("gui/flag_show_constellation_arts_button", true).toBool());
	setFlagShowConstellationBoundariesButton(conf->value("gui/flag_show_boundaries_button", false).toBool());
	setFlagShowAsterismLinesButton(conf->value("gui/flag_show_asterism_lines_button", false).toBool());
	setFlagShowAsterismLabelsButton(conf->value("gui/flag_show_asterism_labels_button", false).toBool());
	setFlagShowQuitButton(conf->value("gui/flag_show_quit_button", true).toBool());
	setFlagShowCardinalButton(conf->value("gui/flag_show_cardinal_button", true).toBool());
	setFlagShowCompassButton(conf->value("gui/flag_show_compass_button", false).toBool());

	setFlagEnableFocusOnDaySpinner(conf->value("gui/flag_focus_day_spinner", false).toBool());

	///////////////////////////////////////////////////////////////////////
	// Create the main base widget
	skyGui->init(this);
	QGraphicsGridLayout* l = new QGraphicsGridLayout();
	l->setContentsMargins(0,0,0,0);
	l->setSpacing(0);
	l->addItem(skyGui, 0, 0);
	atopLevelGraphicsWidget->setLayout(l);

	connect(&StelApp::getInstance(), &StelApp::guiFontSizeChanged, this, &StelGui::updateStelStyle);
	updateStelStyle();

	int margin = conf->value("gui/space_between_groups", 5).toInt();
	skyGui->bottomBar->setGroupMargin("020-gridsGroup", margin, 0);
	skyGui->bottomBar->setGroupMargin("030-landscapeGroup", margin, 0);
	skyGui->bottomBar->setGroupMargin("040-nebulaeGroup", margin, 0);
	skyGui->bottomBar->setGroupMargin("060-othersGroup", margin, margin);
	skyGui->bottomBar->setGroupMargin("070-timeGroup", margin, 0);
	skyGui->bottomBar->setGroupMargin("080-quitGroup", margin, 0);

	skyGui->setGeometry(atopLevelGraphicsWidget->geometry());
	skyGui->updateBarsPos();

	// The disabled text for checkboxes is embossed with the QPalette::Light setting for the ColorGroup Disabled.
	// It doesn't appear to be possible to set this from the stylesheet.  Instead we'll make it 100% transparent
	// and set the text color for disabled in the stylesheets.
	QPalette p = QGuiApplication::palette();
	p.setColor(QPalette::Disabled, QPalette::Light, QColor(0,0,0,0));

	// And this is for the focus...  apparently the focus indicator is the inverted value for Active/Button.
	p.setColor(QPalette::Active, QPalette::Button, QColor(255,255,255));
	QGuiApplication::setPalette(p);
	
	// Set UI language when app is started
	updateI18n();

	StelApp *app = &StelApp::getInstance();
	connect(app, SIGNAL(languageChanged()), this, SLOT(updateI18n()));
	connect(app, SIGNAL(colorSchemeChanged(const QString&)), this, SLOT(setStelStyle(const QString&)));
	initDone = true;
}

void StelGui::addButtonOnBottomBar(QString buttonName, QString actionName, QString groupName)
{
	QPixmap pxmapGlow(":/graphicGui/miscGlow32x32.png");
	QPixmap pxmapOn = QPixmap(QString(":/graphicGui/%1-on.png").arg(buttonName));
	QPixmap pxmapOff = QPixmap(QString(":/graphicGui/%1-off.png").arg(buttonName));
	StelButton* b = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow, actionName);
	skyGui->bottomBar->addButton(b, groupName);
}

void StelGui::addButtonOnLeftBar(QString buttonName, QString actionName)
{
	QPixmap pxmapGlow(":/graphicGui/miscGlow.png");
	QPixmap pxmapOn = QPixmap(QString(":/graphicGui/%1-on.png").arg(buttonName));
	QPixmap pxmapOff = QPixmap(QString(":/graphicGui/%1-off.png").arg(buttonName));
	StelButton* b = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow, actionName);
	skyGui->leftBar->addButton(b);
}

void StelGui::quit()
{
	StelApp::getInstance().quit();
}

//! Reload the current Qt Style Sheet (Debug only)
void StelGui::reloadStyle()
{
	setStelStyle(StelApp::getInstance().getCurrentStelStyle());
}

//! Load color scheme from the given ini file and section name
//! section "default" is the code name of the default style. Any other name is interpreted as CSS filename.
void StelGui::setStelStyle(const QString& style)
{
	// Load the style sheets
	QString qtStyleFileName = (style=="default" ? ":/graphicGui/normalStyle.css" : StelFileMgr::findFile(style+".css"));
	if (qtStyleFileName.isEmpty())
	{
		qWarning() << "Cannot find " << style << ".css. Loading default style.";
		qtStyleFileName = ":/graphicGui/normalStyle.css";
	}

	// Load Qt style sheet
	QFile styleFile(qtStyleFileName);
	if(styleFile.open(QIODevice::ReadOnly))
	{
		qInfo().noquote() << "Loading style file:" << styleFile.fileName();
		currentStelStyle.qtStyleSheet = styleFile.readAll();
		styleFile.close();
	}
	else
		qDebug().noquote() << "Cannot find style file:" << qtStyleFileName;

	QString htmlStyleFileName = (style=="default" ? ":/graphicGui/normalHtml.css" : StelFileMgr::findFile(style+"Html.css"));
	if (htmlStyleFileName.isEmpty())
	{
		qWarning().noquote() << "Cannot find" << style << "Html.css. Loading default HTML style.";
		htmlStyleFileName = ":/graphicGui/normalHtml.css";
	}

	QFile htmlStyleFile(htmlStyleFileName);
	if(htmlStyleFile.open(QIODevice::ReadOnly))
	{
		currentStelStyle.htmlStyleSheet = htmlStyleFile.readAll();
		htmlStyleFile.close();
	}
	else
		qDebug().noquote() << "Cannot find HTML style file:" << htmlStyleFileName;

	const auto scale = double(StelApp::getInstance().getGuiFontSize()) / StelApp::getDefaultGuiFontSize();
	currentStelStyle.qtStyleSheet = applyScaleToCSS(currentStelStyle.qtStyleSheet, scale);
	emit guiStyleChanged(currentStelStyle.qtStyleSheet);
	emit htmlStyleChanged(currentStelStyle.htmlStyleSheet);
}


void StelGui::updateI18n()
{
	// Translate all action texts
	for (auto* obj : StelMainView::getInstance().children())
	{
		QAction* a = qobject_cast<QAction*>(obj);
		if (a)
		{
			const QString& englishText = a->property("englishText").toString();
			if (!englishText.isEmpty())
			{
				a->setText(q_(englishText));
			}
		}
	}
}

void StelGui::update()
{
	StelCore* core = StelApp::getInstance().getCore();
	if (core->getTimeRate()<-0.99*StelCore::JD_SECOND) {
        if ( ! buttonTimeRewind->isChecked())
			buttonTimeRewind->setChecked(true);
	} else {
        if (buttonTimeRewind->isChecked())
			buttonTimeRewind->setChecked(false);
	}
	if (core->getTimeRate()>1.01*StelCore::JD_SECOND) {
        if ( ! buttonTimeForward->isChecked()) {
			buttonTimeForward->setChecked(true);
		}
	} else {
        if (buttonTimeForward->isChecked())
			buttonTimeForward->setChecked(false);
	}
	if (core->getTimeRate() == 0.) {
		if (buttonTimeRealTimeSpeed->isChecked() != StelButton::ButtonStateNoChange)
			buttonTimeRealTimeSpeed->setChecked(StelButton::ButtonStateNoChange);
	} else if (core->getRealTimeSpeed()) {
		if (buttonTimeRealTimeSpeed->isChecked() != StelButton::ButtonStateOn)
			buttonTimeRealTimeSpeed->setChecked(StelButton::ButtonStateOn);
	} else if (buttonTimeRealTimeSpeed->isChecked() != StelButton::ButtonStateOff) {
		buttonTimeRealTimeSpeed->setChecked(StelButton::ButtonStateOff);
	}
	const bool isTimeNow=core->getIsTimeNow() && core->getRealTimeSpeed();
	if (static_cast<bool>(buttonTimeCurrent->isChecked())!=isTimeNow) {
		buttonTimeCurrent->setChecked(isTimeNow);
	}
	StelMovementMgr* mmgr = GETSTELMODULE(StelMovementMgr);
	const bool b = mmgr->getFlagTracking();
	if (static_cast<bool>(buttonGotoSelectedObject->isChecked())!=b) {
		buttonGotoSelectedObject->setChecked(b);
	}

	StelPropertyMgr* propMgr = StelApp::getInstance().getStelPropertyManager();
	bool flag;

	flag = propMgr->getProperty("StelSkyLayerMgr.flagShow")->getValue().toBool();
	if (getAction("actionShow_DSO_Textures")->isChecked() != flag)
		getAction("actionShow_DSO_Textures")->setChecked(flag);

	flag = propMgr->getProperty("HipsMgr.flagShow")->getValue().toBool();
	if (getAction("actionShow_Hips_Surveys")->isChecked() != flag)
		getAction("actionShow_Hips_Surveys")->setChecked(flag);

	flag = propMgr->getProperty("ToastMgr.surveyDisplayed")->getValue().toBool();
	if (getAction("actionShow_Toast_Survey")->isChecked() != flag)
		getAction("actionShow_Toast_Survey")->setChecked(flag);

	flag = propMgr->getProperty("GridLinesMgr.equatorJ2000GridDisplayed")->getValue().toBool();
	if (getAction("actionShow_Equatorial_J2000_Grid")->isChecked() != flag)
		getAction("actionShow_Equatorial_J2000_Grid")->setChecked(flag);

	flag = propMgr->getProperty("GridLinesMgr.galacticGridDisplayed")->getValue().toBool();
	if (getAction("actionShow_Galactic_Grid")->isChecked() != flag)
		getAction("actionShow_Galactic_Grid")->setChecked(flag);

	flag = propMgr->getProperty("GridLinesMgr.eclipticGridDisplayed")->getValue().toBool();
	if (getAction("actionShow_Ecliptic_Grid")->isChecked() != flag)
		getAction("actionShow_Ecliptic_Grid")->setChecked(flag);

	flag = propMgr->getProperty("ConstellationMgr.artDisplayed")->getValue().toBool();
	if (getAction("actionShow_Constellation_Art")->isChecked() != flag)
		getAction("actionShow_Constellation_Art")->setChecked(flag);

	flag = propMgr->getProperty("ConstellationMgr.boundariesDisplayed")->getValue().toBool();
	if (getAction("actionShow_Constellation_Boundaries")->isChecked() != flag)
		getAction("actionShow_Constellation_Boundaries")->setChecked(flag);

	flag = propMgr->getProperty("AsterismMgr.linesDisplayed")->getValue().toBool();
	if (getAction("actionShow_Asterism_Lines")->isChecked() != flag)
		getAction("actionShow_Asterism_Lines")->setChecked(flag);

	flag = propMgr->getProperty("AsterismMgr.namesDisplayed")->getValue().toBool();
	if (getAction("actionShow_Asterism_Labels")->isChecked() != flag)
		getAction("actionShow_Asterism_Labels")->setChecked(flag);

	flag = propMgr->getProperty("LandscapeMgr.cardinalPointsDisplayed")->getValue().toBool();
	if (getAction("actionShow_Cardinal_Points")->isChecked() != flag)
		getAction("actionShow_Cardinal_Points")->setChecked(flag);

	flag = propMgr->getProperty("SpecialMarkersMgr.compassMarksDisplayed")->getValue().toBool();
	if (getAction("actionShow_Compass_Marks")->isChecked() != flag)
		getAction("actionShow_Compass_Marks")->setChecked(flag);

	flag = StelApp::getInstance().getVisionModeNight();
	if (getAction("actionShow_Night_Mode")->isChecked() != flag)
		getAction("actionShow_Night_Mode")->setChecked(flag);

	flag = StelMainView::getInstance().isFullScreen();
	if (getAction("actionSet_Full_Screen_Global")->isChecked() != flag)
		getAction("actionSet_Full_Screen_Global")->setChecked(flag);

	// to avoid crash when observer is on the spaceship
	const QList<StelObjectP> selected = GETSTELMODULE(StelObjectMgr)->getSelectedObject();
	skyGui->infoPanel->setTextFromObjects(selected);

	// Check if the progressbar window changed, if yes update the whole view
	if (savedProgressBarSize!=skyGui->progressBarMgr->boundingRect().size())
	{
		savedProgressBarSize=skyGui->progressBarMgr->boundingRect().size();
		forceRefreshGui();
	}
}

void StelGui::displayAllInfo()
{
	setInfoTextFilters(StelObject::InfoStringGroup(StelObject::AllInfo));
	emit infoStringChanged();
}

void StelGui::displayDefaultInfo()
{
	setInfoTextFilters(StelObject::InfoStringGroup(StelObject::DefaultInfo));
	emit infoStringChanged();
}

void StelGui::displayShortInfo()
{
	setInfoTextFilters(StelObject::InfoStringGroup(StelObject::ShortInfo));
	emit infoStringChanged();
}

void StelGui::displayNoneInfo()
{
	setInfoTextFilters(StelObject::InfoStringGroup(StelObject::None));
	emit infoStringChanged();
}

void StelGui::displayCustomInfo()
{
	setInfoTextFilters(GETSTELMODULE(StelObjectMgr)->getCustomInfoStrings());
	emit infoStringChanged();
}

#ifdef ENABLE_SCRIPTING
void StelGui::setScriptKeys(bool b)
{
	// Allows use of buttons from conf! Bug LP:1530567 -- GZ
	// The swap of keys happens first immediately after program start by execution of startup.ssc
	// Allow different script keys if you properly configure them!

	QSettings* conf = StelApp::getInstance().getSettings();
	Q_ASSERT(conf);

	if (b)
	{
		scriptSaveSpeedbuttons.clear();
		scriptSaveSpeedbuttons << getAction("actionDecrease_Time_Speed")->getShortcut().toString(QKeySequence::PortableText)
				       << getAction("actionIncrease_Time_Speed")->getShortcut().toString(QKeySequence::PortableText)
				       << getAction("actionSet_Real_Time_Speed")->getShortcut().toString(QKeySequence::PortableText);

		// During script execution we disable normal time control.
		// If script keys have not been configured, take those time control keys.
		getAction("actionDecrease_Time_Speed")->setShortcut("");
		getAction("actionIncrease_Time_Speed")->setShortcut("");
		getAction("actionSet_Real_Time_Speed")->setShortcut("");
		QString str;
		str=conf->value("shortcuts/actionDecrease_Script_Speed",scriptSaveSpeedbuttons.at(0)).toString();
		getAction("actionDecrease_Script_Speed")->setShortcut(str);
		str=conf->value("shortcuts/actionIncrease_Script_Speed",scriptSaveSpeedbuttons.at(1)).toString();
		getAction("actionIncrease_Script_Speed")->setShortcut(str);
		str=conf->value("shortcuts/actionSet_Real_Script_Speed",scriptSaveSpeedbuttons.at(2)).toString();
		getAction("actionSet_Real_Script_Speed")->setShortcut(str);
	}
	else
	{
		if (scriptSaveSpeedbuttons.length()<3)
		{
			scriptSaveSpeedbuttons.clear();
			scriptSaveSpeedbuttons << "J" << "L" << "K";
			qWarning() << "StelGui: Saved speed buttons reset to J/K/L";
			qWarning() << "         This is very odd, should not happen.";
		}
		// It is only safe to clear the script speed keys if they are the same as the regular speed keys
		// i.e. if they had been set before running the script.
		if (getAction("actionDecrease_Script_Speed")->getShortcut().toString() == scriptSaveSpeedbuttons.at(0))
			getAction("actionDecrease_Script_Speed")->setShortcut("");
		if (getAction("actionIncrease_Script_Speed")->getShortcut().toString() == scriptSaveSpeedbuttons.at(1))
			getAction("actionIncrease_Script_Speed")->setShortcut("");
		if (getAction("actionSet_Real_Script_Speed")->getShortcut().toString() == scriptSaveSpeedbuttons.at(2))
			getAction("actionSet_Real_Script_Speed")->setShortcut("");
		getAction("actionDecrease_Time_Speed")->setShortcut(scriptSaveSpeedbuttons.at(0));
		getAction("actionIncrease_Time_Speed")->setShortcut(scriptSaveSpeedbuttons.at(1));
		getAction("actionSet_Real_Time_Speed")->setShortcut(scriptSaveSpeedbuttons.at(2));
	}
}

void StelGui::increaseScriptSpeed()
{
	StelApp::getInstance().getScriptMgr().setScriptRate(StelApp::getInstance().getScriptMgr().getScriptRate()*2);
}

void StelGui::decreaseScriptSpeed()
{	
	StelApp::getInstance().getScriptMgr().setScriptRate(StelApp::getInstance().getScriptMgr().getScriptRate()/2);
}

void StelGui::setRealScriptSpeed()
{	
	StelApp::getInstance().getScriptMgr().setScriptRate(1);
}

void StelGui::stopScript()
{	
	StelApp::getInstance().getScriptMgr().stopScript();
}

#ifndef ENABLE_SCRIPT_QML
void StelGui::pauseScript()
{	
	StelApp::getInstance().getScriptMgr().pauseScript();
}

void StelGui::resumeScript()
{	
	StelApp::getInstance().getScriptMgr().resumeScript();
}
#endif
#endif

void StelGui::setFlagShowFlipButtons(bool b)
{
	if (b!=flagShowFlipButtons)
	{
		if (b==true)
		{
			if (flipVert==nullptr)
			{
				// Create the vertical flip button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btFlipVertical-on.png");
				QPixmap pxmapOff(":/graphicGui/btFlipVertical-off.png");
				flipVert = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionVertical_Flip");
			}
			if (flipHoriz==nullptr)
			{
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btFlipHorizontal-on.png");
				QPixmap pxmapOff(":/graphicGui/btFlipHorizontal-off.png");
				flipHoriz = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionHorizontal_Flip");
			}
			getButtonBar()->addButton(flipVert, "060-othersGroup", "actionQuit_Global");
			getButtonBar()->addButton(flipHoriz, "060-othersGroup", "actionVertical_Flip");
		} else {
			getButtonBar()->hideButton("actionVertical_Flip");
			getButtonBar()->hideButton("actionHorizontal_Flip");
		}
		flagShowFlipButtons = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_flip_buttons", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowFlipButtonsChanged(b);
	}
}

// Define whether the button toggling nebulae background images should be visible
void StelGui::setFlagShowNebulaBackgroundButton(bool b)
{
	if (b!=flagShowNebulaBackgroundButton)
	{
		if (b==true)
		{
			if (btShowNebulaeBackground==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btNebulaeBackground-on.png");
				QPixmap pxmapOff(":/graphicGui/btNebulaeBackground-off.png");
				btShowNebulaeBackground = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_DSO_Textures");
			}
			getButtonBar()->addButton(btShowNebulaeBackground, "040-nebulaeGroup");
		} else {
			getButtonBar()->hideButton("actionShow_DSO_Textures");
		}
		flagShowNebulaBackgroundButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_nebulae_background_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowNebulaBackgroundButtonChanged(b);
	}
}

// Define whether the button toggling observing list should be visible
void StelGui::setFlagShowObsListButton(bool b)
{
	if(b!=flagShowObsListButton)
	{
		if (b==true) {
			if (btShowObsList==nullptr) {
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btObsList-on.png");
				QPixmap pxmapOff(":/graphicGui/btObsList-off.png");
				btShowObsList = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_ObsList_Window_Global");
			}
			getButtonBar()->addButton(btShowObsList, "060-othersGroup");
		} else {
			getButtonBar()->hideButton("actionShow_ObsList_Window_Global");
		}
		flagShowObsListButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_obslist_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowObsListButtonChanged(b);
	}
}

// Define whether the button toggling ICRS grid should be visible
void StelGui::setFlagShowICRSGridButton(bool b)
{
	if (b!=flagShowICRSGridButton)
	{
		if (b==true)
		{
			if (btShowICRSGrid==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btEquatorialJ2000Grid-on.png");
				QPixmap pxmapOff(":/graphicGui/btEquatorialJ2000Grid-off.png");
				btShowICRSGrid = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Equatorial_J2000_Grid");
			}
			getButtonBar()->addButton(btShowICRSGrid, "020-gridsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Equatorial_J2000_Grid");
		}
		flagShowICRSGridButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_icrs_grid_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowICRSGridButtonChanged(b);
	}
}

// Define whether the button toggling galactic grid should be visible
void StelGui::setFlagShowGalacticGridButton(bool b)
{
	if (b!=flagShowGalacticGridButton)
	{
		if (b==true)
		{
			if (btShowGalacticGrid==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btGalacticGrid-on.png");
				QPixmap pxmapOff(":/graphicGui/btGalacticGrid-off.png");
				btShowGalacticGrid = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Galactic_Grid");
			}
			getButtonBar()->addButton(btShowGalacticGrid, "020-gridsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Galactic_Grid");
		}
		flagShowGalacticGridButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_galactic_grid_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowGalacticGridButtonChanged(b);
	}
}

// Define whether the button toggling ecliptic grid should be visible
void StelGui::setFlagShowEclipticGridButton(bool b)
{
	if (b!=flagShowEclipticGridButton)
	{
		if (b==true)
		{
			if (btShowEclipticGrid==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btEclipticGrid-on.png");
				QPixmap pxmapOff(":/graphicGui/btEclipticGrid-off.png");
				btShowEclipticGrid = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Ecliptic_Grid");
			}
			getButtonBar()->addButton(btShowEclipticGrid, "020-gridsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Ecliptic_Grid");
		}
		flagShowEclipticGridButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_ecliptic_grid_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowEclipticGridButtonChanged(b);
	}
}

// Define whether the button toggling constellation boundaries should be visible
void StelGui::setFlagShowConstellationBoundariesButton(bool b)
{
	if (b!=flagShowConstellationBoundariesButton)
	{
		if (b==true)
		{
			if (btShowConstellationBoundaries==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btConstellationBoundaries-on.png");
				QPixmap pxmapOff(":/graphicGui/btConstellationBoundaries-off.png");
				btShowConstellationBoundaries = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Constellation_Boundaries");
			}
			getButtonBar()->addButton(btShowConstellationBoundaries, "010-constellationsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Constellation_Boundaries");
		}
		flagShowConstellationBoundariesButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_boundaries_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowConstellationBoundariesButtonChanged(b);
	}
}

// Define whether the button toggling constellation arts should be visible
void StelGui::setFlagShowConstellationArtsButton(bool b)
{
	if (b!=flagShowConstellationArtsButton)
	{
		if (b==true)
		{
			if (btShowConstellationArts==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btConstellationArt-on.png");
				QPixmap pxmapOff(":/graphicGui/btConstellationArt-off.png");
				btShowConstellationArts = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Constellation_Art");
			}
			getButtonBar()->addButton(btShowConstellationArts, "010-constellationsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Constellation_Art");
		}
		flagShowConstellationArtsButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_constellation_arts_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowConstellationArtsButtonChanged(b);
	}
}

// Define whether the button toggling asterism lines should be visible
void StelGui::setFlagShowAsterismLinesButton(bool b)
{
	if (b!=flagShowAsterismLinesButton)
	{
		if (b==true)
		{
			if (btShowAsterismLines==nullptr)
			{
				// Create the asterism lines button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btAsterismLines-on.png");
				QPixmap pxmapOff(":/graphicGui/btAsterismLines-off.png");
				btShowAsterismLines = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Asterism_Lines");
			}
			getButtonBar()->addButton(btShowAsterismLines, "010-constellationsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Asterism_Lines");
		}
		flagShowAsterismLinesButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_asterism_lines_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowAsterismLinesButtonChanged(b);
	}
}

// Define whether the button toggling asterism labels should be visible
void StelGui::setFlagShowAsterismLabelsButton(bool b)
{
	if (b!=flagShowAsterismLabelsButton)
	{
		if (b==true)
		{
			if (btShowAsterismLabels==nullptr)
			{
				// Create the asterism labels button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btAsterismLabels-on.png");
				QPixmap pxmapOff(":/graphicGui/btAsterismLabels-off.png");
				btShowAsterismLabels = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Asterism_Labels");
			}
			getButtonBar()->addButton(btShowAsterismLabels, "010-constellationsGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Asterism_Labels");
		}
		flagShowAsterismLabelsButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_asterism_labels_button", b);
		conf->sync();
		if (initDone) {
			skyGui->updateBarsPos();
		}
		emit flagShowAsterismLabelsButtonChanged(b);
	}
}


// Define whether the button toggling DSS images should be visible
// We keep Toast even though HiPS is now available: We have a local Toast option better suited for offline operation!
void StelGui::setFlagShowDSSButton(bool b)
{
	if (b!=flagShowDSSButton)
	{
		if (b==true)
		{
			if (btShowDSS==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btDSS-on.png");
				QPixmap pxmapOff(":/graphicGui/btDSS-off");
				btShowDSS = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Toast_Survey");
			}
			getButtonBar()->addButton(btShowDSS, "040-nebulaeGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Toast_Survey");
		}
		flagShowDSSButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_dss_button", b);
		conf->sync();
		emit flagShowDSSButtonChanged(b);
	}
}

// Define whether the button toggling HiPS images should be visible
void StelGui::setFlagShowHiPSButton(bool b)
{
	if (b!=flagShowHiPSButton)
	{
		if (b==true)
		{
			if (btShowHiPS==nullptr)
			{
				// Create the nebulae background button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btHIPS-on.png");
				QPixmap pxmapOff(":/graphicGui/btHIPS-off.png");
				btShowHiPS = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Hips_Surveys", false, "actionShow_Hips_Surveys_dialog");
			}
			getButtonBar()->addButton(btShowHiPS, "040-nebulaeGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Hips_Surveys");
		}
		flagShowHiPSButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_hips_button", b);
		conf->sync();
		emit flagShowHiPSButtonChanged(b);
	}
}

// Define whether the button to center on selected object should be visible
void StelGui::setFlagShowGotoSelectedObjectButton(bool b)
{
	if (b!=flagShowGotoSelectedObjectButton)
	{
		if (b==true)
		{
			if (buttonGotoSelectedObject==nullptr)
			{
				// Create the button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btGotoSelectedObject-on.png");
				QPixmap pxmapOff(":/graphicGui/btGotoSelectedObject-off.png");
				buttonGotoSelectedObject = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionGoto_Selected_Object");
			}
			getButtonBar()->addButton(buttonGotoSelectedObject, "060-othersGroup");
		} else {
			getButtonBar()->hideButton("actionGoto_Selected_Object");
		}
		flagShowGotoSelectedObjectButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_goto_selected_button", b);
		conf->sync();
		emit flagShowGotoSelectedObjectButtonChanged(b);
	}
}
// Define whether the button toggling night mode should be visible
void StelGui::setFlagShowNightmodeButton(bool b)
{
	if (b!=flagShowNightmodeButton)
	{
		if (b==true)
		{
			if (buttonNightmode==nullptr)
			{
				// Create the nightmode button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btNightView-on.png");
				QPixmap pxmapOff(":/graphicGui/btNightView-off.png");
				buttonNightmode = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Night_Mode");
			}
			getButtonBar()->addButton(buttonNightmode, "060-othersGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Night_Mode");
		}
		flagShowNightmodeButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_nightmode_button", b);
		conf->sync();
		emit flagShowNightmodeButtonChanged(b);
	}
}
// Define whether the button toggling fullscreen mode should be visible
void StelGui::setFlagShowFullscreenButton(bool b)
{
	if (b!=flagShowFullscreenButton)
	{
		if (b==true)
		{
			if (buttonFullscreen==nullptr)
			{
				// Create the fullscreen button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btFullScreen-on.png");
				QPixmap pxmapOff(":/graphicGui/btFullScreen-off.png");
				buttonFullscreen = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionSet_Full_Screen_Global");
			}
			getButtonBar()->addButton(buttonFullscreen, "060-othersGroup");
		} else {
			getButtonBar()->hideButton("actionSet_Full_Screen_Global");
		}
		flagShowFullscreenButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_fullscreen_button", b);
		conf->sync();
		emit flagShowFullscreenButtonChanged(b);
	}
}
// Define whether the quit button should be visible
void StelGui::setFlagShowQuitButton(bool b)
{
	if (b!=flagShowQuitButton)
	{
		if (b==true) {
			if (buttonFullscreen==nullptr)
			{
				// Create the fullscreen button
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btQuit.png");
				buttonQuit = new StelButton(nullptr, pxmapOn, pxmapOn, pxmapGlow32x32, "actionQuit_Global");
			}
			getButtonBar()->addButton(buttonQuit, "080-quitGroup");
		} else {
			getButtonBar()->hideButton("actionQuit_Global");
		}
		flagShowQuitButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_quit_button", b);
		conf->sync();
		emit flagShowQuitButtonChanged(b);
	}
}

void StelGui::setFlagShowCardinalButton(bool b)
{
	if (b!=flagShowCardinalButton)
	{
		if (b==true) {
			if (btShowCardinal==nullptr)
			{
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btCardinalPoints-on.png");
				QPixmap pxmapOff(":/graphicGui/btCardinalPoints-off");
				btShowCardinal = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Cardinal_Points");
			}
			getButtonBar()->addButton(btShowCardinal, "030-landscapeGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Cardinal_Points");
		}
		flagShowCardinalButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_cardinal_button", b);
		conf->sync();
		emit flagShowCardinalButtonChanged(b);
	}
}

void StelGui::setFlagShowCompassButton(bool b)
{
	if (b!=flagShowCompassButton)
	{
		if (b==true)
		{
			if (btShowCompass==nullptr)
			{
				QPixmap pxmapGlow32x32(":/graphicGui/miscGlow32x32.png");
				QPixmap pxmapOn(":/graphicGui/btCompass-on.png");
				QPixmap pxmapOff(":/graphicGui/btCompass-off");
				btShowCompass = new StelButton(nullptr, pxmapOn, pxmapOff, pxmapGlow32x32, "actionShow_Compass_Marks");
			}
			getButtonBar()->addButton(btShowCompass, "030-landscapeGroup");
		} else {
			getButtonBar()->hideButton("actionShow_Compass_Marks");
		}
		flagShowCompassButton = b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_compass_button", b);
		conf->sync();
		emit flagShowCompassButtonChanged(b);
	}
}

void StelGui::setFlagUseButtonsBackground(bool b)
{
	if (b!=flagUseButtonsBackground)
	{
		flagUseButtonsBackground=b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_show_buttons_background", b);
		conf->sync();
		emit flagUseButtonsBackgroundChanged(b);
	}
}

void StelGui::setFlagUseKineticScrolling(bool b)
{
	if (b!=flagUseKineticScrolling)
	{
		flagUseKineticScrolling=b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_enable_kinetic_scrolling", b);
		conf->sync();
		emit flagUseKineticScrollingChanged(b);
	}
}

void StelGui::setFlagEnableFocusOnDaySpinner(bool b)
{
	if (b!=flagEnableFocusOnDaySpinner)
	{
		flagEnableFocusOnDaySpinner=b;
		QSettings* conf = StelApp::getInstance().getSettings();
		Q_ASSERT(conf);
		conf->setValue("gui/flag_focus_day_spinner", b);
		conf->sync();
		emit flagEnableFocusOnDaySpinnerChanged(b);
	}
}

void StelGui::setVisible(bool b)
{
	skyGui->setVisible(b);	
	emit visibleChanged(b);
}

bool StelGui::getVisible() const
{
	return skyGui->isVisible();
}

bool StelGui::isCurrentlyUsed() const
{
	return skyGui->bottomBar->isUnderMouse() || skyGui->leftBar->isUnderMouse();
}

void StelGui::setInfoTextFilters(const StelObject::InfoStringGroup& aflags)
{
	skyGui->infoPanel->setInfoTextFilters(aflags);
}

const StelObject::InfoStringGroup& StelGui::getInfoTextFilters() const
{
	return skyGui->infoPanel->getInfoTextFilters();
}

BottomStelBar* StelGui::getButtonBar() const
{
	return skyGui->bottomBar;
}

LeftStelBar* StelGui::getWindowsButtonBar() const
{
	return skyGui->leftBar;
}

SkyGui* StelGui::getSkyGui() const
{
	return skyGui;
}

bool StelGui::getAutoHideHorizontalButtonBar() const
{
	return skyGui->autoHideBottomBar;
}

void StelGui::setAutoHideHorizontalButtonBar(bool b)
{
	if (skyGui->autoHideBottomBar!=b)
	{
		skyGui->autoHideBottomBar=b;
		StelApp::immediateSave("gui/auto_hide_horizontal_toolbar", b);
		emit autoHideHorizontalButtonBarChanged(b);
	}
}

bool StelGui::getAutoHideVerticalButtonBar() const
{
	return skyGui->autoHideLeftBar;
}

void StelGui::setAutoHideVerticalButtonBar(bool b)
{
	if (skyGui->autoHideLeftBar!=b)
	{
		skyGui->autoHideLeftBar=b;
		StelApp::immediateSave("gui/auto_hide_vertical_toolbar", b);
		emit autoHideVerticalButtonBarChanged(b);
	}
}

bool StelGui::getFlagShowQuitButton() const
{
	return flagShowQuitButton;
}

bool StelGui::getFlagShowFlipButtons() const
{
	return flagShowFlipButtons;
}

bool StelGui::getFlagShowNebulaBackgroundButton() const
{
	return flagShowNebulaBackgroundButton;
}

bool StelGui::getFlagShowDSSButton() const
{
	return flagShowDSSButton;
}

bool StelGui::getFlagShowHiPSButton() const
{
	return flagShowHiPSButton;
}

bool StelGui::getFlagShowGotoSelectedObjectButton() const
{
	return flagShowGotoSelectedObjectButton;
}

bool StelGui::getFlagShowNightmodeButton() const
{
	return flagShowNightmodeButton;
}

bool StelGui::getFlagShowFullscreenButton() const
{
	return flagShowFullscreenButton;
}

bool StelGui::getFlagShowObsListButton() const
{
	return flagShowObsListButton;
}

bool StelGui::getFlagShowICRSGridButton() const
{
	return flagShowICRSGridButton;
}

bool StelGui::getFlagShowGalacticGridButton() const
{
	return flagShowGalacticGridButton;
}

bool StelGui::getFlagShowEclipticGridButton() const
{
	return flagShowEclipticGridButton;
}

bool StelGui::getFlagShowConstellationBoundariesButton() const
{
	return flagShowConstellationBoundariesButton;
}

bool StelGui::getFlagShowConstellationArtsButton() const
{
	return flagShowConstellationArtsButton;
}

bool StelGui::getFlagShowAsterismLinesButton() const
{
	return flagShowAsterismLinesButton;
}

bool StelGui::getFlagShowAsterismLabelsButton() const
{
	return flagShowAsterismLabelsButton;
}

bool StelGui::getFlagShowCardinalButton() const
{
	return flagShowCardinalButton;
}

bool StelGui::getFlagShowCompassButton() const
{
	return flagShowCompassButton;
}

bool StelGui::initComplete(void) const
{
	return initDone;
}

void StelGui::forceRefreshGui()
{
	skyGui->updateBarsPos();
}

#ifdef ENABLE_SCRIPTING
void StelGui::scriptStarted()
{
	setScriptKeys(true);
}

void StelGui::scriptStopped()
{
	setScriptKeys(false);
}
#endif

void StelGui::setGuiVisible(bool b)
{
	setVisible(b);
	emit visibleChanged(b);
}

StelAction* StelGui::getAction(const QString& actionName) const
{
	return StelApp::getInstance().getStelActionManager()->findAction(actionName);
}

void StelGui::copySelectedObjectInfo(void)
{
	const auto cb = QGuiApplication::clipboard();
	const auto md = new QMimeData;
	const auto colorRE = QRegularExpression(" color=\"?#[0-9a-f]+\"?");
	md->setHtml(skyGui->infoPanel->getSelectedHTML().replace(colorRE, ""));
	md->setText(skyGui->infoPanel->getSelectedText());
	cb->setMimeData(md);
}

bool StelGui::getAstroCalcVisible() const
{
	return astroCalcDialog && astroCalcDialog->visible();
}
