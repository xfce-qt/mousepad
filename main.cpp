#include <QApplication>
#include <QComboBox>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QMenuBar>
#include <QKeySequence>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QToolBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFontDialog>
#include <QSettings>
#include <QErrorMessage>
#include "codeeditor.h"

#include <libintl.h>
#include <locale.h>
#define _(STRING) gettext(STRING)

QSettings *settings;

QTabWidget *prefswin; // Menubar > Edit > Preferences
QToolBar *findToolBar; // Menubar > Search > Find
QDialog *findnReplaceWindow; // Menubar > Search > Find and Replace
QDialog *gotoWindow; // Menubar > Search > Go to
QFontDialog *fontWindow; // Menubar > View > Font
CodeEditor *editor;

void initFontWindow(QFontDialog *fontWin){
	QObject::connect(fontWin, &QFontDialog::currentFontChanged, editor->document(), &QTextDocument::setDefaultFont);
	QObject::connect(fontWin, &QFontDialog::fontSelected, [](QFont font){settings->setValue("FontData", font.toString());settings->sync();});
}

void initGotoWindow(QDialog *gotoWin){
	gotoWin->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	gotoWin->setWindowTitle("Go");
	QVBoxLayout *replaceLayout = new QVBoxLayout(gotoWin);
	gotoWin->setLayout(replaceLayout);
	
	QWidget *contents = new QWidget(gotoWin);
	contents->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
	QFormLayout *contentsLayout = new QFormLayout(contents);
	contents->setLayout(contentsLayout);
	replaceLayout->addWidget(contents);
	
	QSpinBox *lineSpinner = new QSpinBox(contents);
	contentsLayout->addRow("Line number (y):", lineSpinner);
	
	QSpinBox *columnSpinner = new QSpinBox(contents);
	contentsLayout->addRow("Column (x):", columnSpinner);
	
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, gotoWin);
	replaceLayout->addWidget(buttonBox);
}

void initFindnReplaceWindow(QDialog *replaceWin){
	replaceWin->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	replaceWin->setWindowTitle("Replace");
	QVBoxLayout *replaceLayout = new QVBoxLayout(replaceWin);
	replaceWin->setLayout(replaceLayout);
	
	QWidget *contents = new QWidget(replaceWin);
	contents->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
	QFormLayout *contentsLayout = new QFormLayout(contents);
	contents->setLayout(contentsLayout);
	replaceLayout->addWidget(contents);
	
	QLineEdit *findEntry = new QLineEdit(contents);
	contentsLayout->addRow("Find:", findEntry);
	
	QLineEdit *replaceEntry = new QLineEdit(contents);
	contentsLayout->addRow("Replace:", replaceEntry);
	
	QCheckBox *matchCaseCheckbox = new QCheckBox("Match case", contents);
	contentsLayout->addRow("", matchCaseCheckbox);
	
	QCheckBox *wholeWordCheckbox = new QCheckBox("Match whole word", contents);
	contentsLayout->addRow("", wholeWordCheckbox);
	
	QCheckBox *regularExpressionCheckbox = new QCheckBox("Regular expression", contents);
	contentsLayout->addRow("", regularExpressionCheckbox);
	
	QComboBox *subjectComboBox = new QComboBox(contents);
	subjectComboBox->addItem("Selection");
	subjectComboBox->addItem("This document");
	subjectComboBox->addItem("All documents");
	subjectComboBox->setCurrentIndex(1);
	contentsLayout->addRow("Replace in", subjectComboBox);
	
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, replaceWin);
	QPushButton *replaceAllBtn = buttonBox->addButton("Replace All", QDialogButtonBox::ApplyRole);
	replaceLayout->addWidget(buttonBox);
}

void initFindToolBar(QToolBar *toolbar){
    QAction *close = new QAction(QIcon::fromTheme("window-close"), "Close");
	QObject::connect(close, &QAction::triggered, toolbar, &QWidget::setVisible);
	toolbar->addAction(close);
	
	QLineEdit *searchBar = new QLineEdit(toolbar);
	toolbar->addWidget(searchBar);
	
	QAction *up = new QAction(QIcon::fromTheme("go-up"), "Previous");
	toolbar->addAction(up);
	QAction *down = new QAction(QIcon::fromTheme("go-down"), "Next");
	toolbar->addAction(down);
	
	QCheckBox *matchCaseCheckbox = new QCheckBox("Match case", toolbar);
	toolbar->addWidget(matchCaseCheckbox);
	
	QCheckBox *regularExpressionCheckbox = new QCheckBox("Regular expression", toolbar);
	toolbar->addWidget(regularExpressionCheckbox);
	
	QLabel *separator = new QLabel(toolbar);
	separator->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	toolbar->addWidget(separator);
}

void initPrefsWindow(QTabWidget *prefsWin){
	QWidget *viewPage = new QWidget(prefsWin);
    prefsWin->addTab(viewPage, "View");
    QHBoxLayout *viewLayout = new QHBoxLayout(viewPage);
    viewPage->setLayout(viewLayout);
    
    QGroupBox *displayGroupBox = new QGroupBox("Display", viewPage);
    QVBoxLayout *displayGroupLayout = new QVBoxLayout(displayGroupBox);
    
    QCheckBox *showLineNumbersCheckbox = new QCheckBox("Show line numbers", displayGroupBox);
    displayGroupLayout->addWidget(showLineNumbersCheckbox);
    
    QCheckBox *displayWhitespaceCheckbox = new QCheckBox("Display whitespace", displayGroupBox);
    displayGroupLayout->addWidget(displayWhitespaceCheckbox);
    
    QCheckBox *displayLineEndingsCheckbox = new QCheckBox("Display line endings", displayGroupBox);
    displayGroupLayout->addWidget(displayLineEndingsCheckbox);
    
    QWidget *rightMarginWidget = new QWidget(displayGroupBox);
    rightMarginWidget->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    QHBoxLayout *rightMarginLayout = new QHBoxLayout(rightMarginWidget);
    rightMarginLayout->setMargin(0);
    rightMarginWidget->setLayout(rightMarginLayout);
    QCheckBox *rightMarginCheckbox = new QCheckBox("Long line margin at column", rightMarginWidget);
    rightMarginLayout->addWidget(rightMarginCheckbox);
    QSpinBox *rightMarginSpinbox = new QSpinBox(rightMarginWidget);
    rightMarginSpinbox->setEnabled(false);
    rightMarginLayout->addWidget(rightMarginSpinbox);
    QObject::connect(rightMarginCheckbox, &QCheckBox::stateChanged, rightMarginSpinbox, &QWidget::setEnabled);
    displayGroupLayout->addWidget(rightMarginWidget);
    
    QCheckBox *highlightCurrentLineCheckbox = new QCheckBox("Highlight current line", displayGroupBox);
    displayGroupLayout->addWidget(highlightCurrentLineCheckbox);
    
    //QCheckBox *showLineNumbersCheckbox = new QCheckBox("Show line numbers", displayGroupBox);
    //displayGroupLayout->addWidget(showLineNumbersCheckbox);
    
    /*
    QCheckBox *fileNameInTitleBarCheckbox = new QCheckBox("Show full filename in title bar", displayGroupBox);
    displayGroupLayout->addWidget(fileNameInTitleBarCheckbox);
    
    QCheckBox *rememberWindowSizeCheckbox = new QCheckBox("Remember window size", displayGroupBox);
    displayGroupLayout->addWidget(rememberWindowSizeCheckbox);
    
    QCheckBox *rememberWindowPosCheckbox = new QCheckBox("Remember window position", displayGroupBox);
    displayGroupLayout->addWidget(rememberWindowPosCheckbox);
    
    QCheckBox *rememberWindowStateCheckbox = new QCheckBox("Remember window state", displayGroupBox);
    rememberWindowStateCheckbox->setToolTip("remember if the window was maxmized/minimized");
    displayGroupLayout->addWidget(rememberWindowStateCheckbox);
    */
    
    viewLayout->addWidget(displayGroupBox);
}

void initMenuBar(QMenuBar *bar){
	/* File... */
	QMenu *fileMenu = bar->addMenu("&File");
	
	QAction *newFileAction = fileMenu->addAction(QIcon::fromTheme("document-new"), "New");
	newFileAction->setShortcut(QKeySequence(QKeySequence::New));
	
	QAction *newWinAction = fileMenu->addAction("New Window");
	newWinAction->setShortcut(QKeySequence(QKeySequence::AddTab));
	
	QAction *newFromTemplateAction = fileMenu->addAction("New From Template");
	
	fileMenu->addSeparator();
	
	QAction *openFileAction = fileMenu->addAction(QIcon::fromTheme("document-open"), "Open");
	newFileAction->setShortcut(QKeySequence(QKeySequence::Open));
	
	QAction *openRecentAction = fileMenu->addAction(QIcon::fromTheme("document-open-recent"), "Open Recent");
	
	fileMenu->addSeparator();
	
	QAction *saveAction = fileMenu->addAction(QIcon::fromTheme("document-save"), "Save");
	saveAction->setShortcut(QKeySequence(QKeySequence::Save));
	
	QAction *saveAsAction = fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save As");
	saveAsAction->setShortcut(QKeySequence(QKeySequence::SaveAs));
	
	QAction *saveAllAction = fileMenu->addAction("Save All");
	
	fileMenu->addSeparator();
	
	QAction *printAction = fileMenu->addAction(QIcon::fromTheme("document-print"), "Print");
	saveAsAction->setShortcut(QKeySequence(QKeySequence::Print));
	
	fileMenu->addSeparator();
	
	QAction *detachTabAction = fileMenu->addAction("Detach Tab");
	
	fileMenu->addSeparator();
	
	QAction *closeTabAction = fileMenu->addAction(QIcon::fromTheme("window-close"), "Close Tab");
	closeTabAction->setShortcut(QKeySequence(QKeySequence::Close));
	
	QAction *closeWindowAction = fileMenu->addAction("Close Window");
	
	QAction *quitAction = fileMenu->addAction(QIcon::fromTheme("application-exit"), "Quit", []{exit(0);});
	quitAction->setShortcut(QKeySequence(QKeySequence::Quit));
	
	
	
	/* Edit... */
	QMenu *editMenu = bar->addMenu("&Edit");
	
	QAction *undoAction = editMenu->addAction(QIcon::fromTheme("edit-undo"), "Undo");
	undoAction->setShortcut(QKeySequence(Qt::CTRL, Qt::Key_Z));
	
	QAction *redoAction = editMenu->addAction(QIcon::fromTheme("edit-redo"), "Redo");
	redoAction->setShortcut(QKeySequence(Qt::CTRL, Qt::Key_Y));
	
	editMenu->addSeparator();
	
	QAction *cutAction = editMenu->addAction(QIcon::fromTheme("edit-cut"), "Cut");
	undoAction->setShortcut(QKeySequence(QKeySequence::Cut));
	
	QAction *copyAction = editMenu->addAction(QIcon::fromTheme("edit-copy"), "Copy");
	copyAction->setShortcut(QKeySequence(QKeySequence::Copy));
	
	QAction *pasteAction = editMenu->addAction(QIcon::fromTheme("edit-paste"), "Paste");
	pasteAction->setShortcut(QKeySequence(QKeySequence::Paste));
	
	editMenu->addSeparator();
	
	QAction *selectAllAction = editMenu->addAction(QIcon::fromTheme("edit-select-all"), "Select All");
	selectAllAction->setShortcut(QKeySequence(QKeySequence::SelectAll));
	
	editMenu->addSeparator();
	
	QMenu *convertMenu = editMenu->addMenu("Convert");
	QAction *convertToUppercase = convertMenu->addAction("To Uppercase");
	QAction *convertToLowercase = convertMenu->addAction("To Lowercase");
	QAction *convertToTitlecase = convertMenu->addAction("To Title Case");
	QAction *convertToOppositecase = convertMenu->addAction("To Opposite Case");
	convertMenu->addSeparator();
	QAction *convertTabsToSpacesAction = convertMenu->addAction("Tabs to Spaces");
	QAction *convertSpacesToTabsAction = convertMenu->addAction("Spaces to Tabs");
	
	QAction *increaseIndentAction = editMenu->addAction(QIcon::fromTheme("format-indent-more"), "Increase Indent");
	increaseIndentAction->setShortcut(QKeySequence(Qt::Key_Tab));
	
	QAction *decreaseIndentAction = editMenu->addAction(QIcon::fromTheme("format-indent-less"), "Decrease Indent");
	decreaseIndentAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Tab));
	
	editMenu->addSeparator();
	
	QAction *preferencesAction = editMenu->addAction(QIcon::fromTheme("preferences-system"), "Preferences...");
	preferencesAction->setShortcut(QKeySequence::Preferences);
	
	/* Search... */
	QMenu *searchMenu = bar->addMenu("&Search");
	
	QAction *findAction = searchMenu->addAction(QIcon::fromTheme("edit-find"), "Find");
	QObject::connect(findAction, &QAction::triggered, [findToolBar]{findToolBar->setVisible(true);});
	findAction->setShortcut(QKeySequence::Find);
	
	QAction *findnReplaceAction = searchMenu->addAction(QIcon::fromTheme("edit-find-replace"), "Find and Replace...");
	QObject::connect(findnReplaceAction, &QAction::triggered, [findnReplaceWindow]{findnReplaceWindow->setVisible(true);});
	findnReplaceAction->setShortcut(QKeySequence::Replace);
	
	QAction *gotoAction = searchMenu->addAction(QIcon::fromTheme("go-next"), "Go to");
	QObject::connect(gotoAction, &QAction::triggered, [gotoWindow]{gotoWindow->setVisible(true);});
	
	
	/* View... */
	QMenu *viewMenu = bar->addMenu("&View");
	
	QAction *fontAction = viewMenu->addAction(QIcon::fromTheme("preferences-desktop-font"), "Font...");
	QObject::connect(fontAction, &QAction::triggered, []{fontWindow->setVisible(true);});
	
	editMenu->addSeparator();
    
    QMenu *colorSchemeMenu = viewMenu->addMenu("Color Scheme");
	
	QAction *lineNumbersAction = viewMenu->addAction("Line numbers");
	lineNumbersAction->setCheckable(true);
	QObject::connect(lineNumbersAction, &QAction::toggled, editor, &CodeEditor::disableLineNumbers);
}

int main(int argc, char** argv){
	QApplication *app = new QApplication(argc, argv);
    QCoreApplication::setOrganizationName("xfce4-qt");
    QCoreApplication::setOrganizationDomain("xfce4.org");
    QCoreApplication::setApplicationName("mousepad");
	
	settings = new QSettings();
	
	QMainWindow *mainwin = new QMainWindow();
	mainwin->setWindowTitle("Mousepad");
	mainwin->setWindowIcon(QIcon("icons/"));
	
	findnReplaceWindow = new QDialog(mainwin);
	initFindnReplaceWindow(findnReplaceWindow);
	findnReplaceWindow->setVisible(false);
	
	gotoWindow = new QDialog(mainwin);
	initGotoWindow(gotoWindow);
	gotoWindow->setVisible(false);
	
	QString fontData = settings->value("FontData").toString();
	QFont font;
	if(!fontData.isNull()){
		if(!(font.fromString(fontData))){
			QErrorMessage badFontDataMsg;
			badFontDataMsg.showMessage("Bad font data stored in config");
			font = QFont("Monospaced");
		}
	} else font = QFont("Monospaced");
	editor = new CodeEditor(font, mainwin);
	mainwin->setCentralWidget(editor);
	
	fontWindow = new QFontDialog(editor->document()->defaultFont(), mainwin);
	initFontWindow(fontWindow);
	fontWindow->setVisible(false);
    
	findToolBar = new QToolBar(mainwin);
	initFindToolBar(findToolBar);
	findToolBar->setVisible(false);
	mainwin->addToolBar(Qt::BottomToolBarArea, findToolBar);
	
	QMenuBar *menubar = new QMenuBar(mainwin);
	initMenuBar(menubar);
	mainwin->setMenuBar(menubar);
	
    prefswin = new QTabWidget();
	prefswin->setWindowFlags(Qt::Dialog);
    initPrefsWindow(prefswin);
    prefswin->show();
    
    mainwin->show();
    
	app->exec();
}
