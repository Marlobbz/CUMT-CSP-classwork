#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "document.h"
#include "findreplacedialog.h"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QList>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

// 程序结构说明：
//   MainWindow 负责“调度”：菜单/工具栏被点击后，找到当前 Document，
//   再把具体操作转发给 Document。
//   Document 负责“干活”：持有 QTextEdit，负责文件读写和格式设置。
//   FindReplaceDialog 负责查找替换，操作当前文档的 QTextEdit。
// 这样主窗口不需要保存任何文本，多个文档之间也不会互相干扰。

namespace {

// 平铺算法：把 MDI 区域平均分给每个子窗口。
// 以水平平铺为例：
//   1. 可用宽度 W 除以窗口数 n，得到每个窗口的基准宽度 w = W / n；
//   2. 整数除法会丢掉余数，所以让“最后一个窗口”把余数全部吃掉：
//      最后一个窗口的宽度 = 右边界 - 它的起点 + 1。
// 这样窗口之间既不重叠，也不会在右边留下空白。
// 垂直平铺同理，只是把宽度换成高度、x 换成 y。
void tileMdiWindows(QMdiArea *area, Qt::Orientation orientation)
{
    const QList<QMdiSubWindow *> windows = area->subWindowList();
    if (windows.isEmpty())
        return; // 没有窗口，直接返回，避免下面除以 0

    const QRect areaRect = area->viewport()->rect();
    const int count = windows.size();

    if (orientation == Qt::Horizontal) {
        const int width = areaRect.width() / count; // 基准宽度（向下取整）

        for (int i = 0; i < count; ++i) {
            QMdiSubWindow *window = windows.at(i);

            // 最大化状态下 setGeometry() 不生效，先恢复成普通状态。
            if (window->isMaximized())
                window->setWindowState(window->windowState() & ~Qt::WindowMaximized);

            const int x = areaRect.left() + i * width;

            // 最后一个窗口吸收余数；qMax(1, w) 防止窗口太多时算出 0 宽。
            const int w = (i == count - 1) ? (areaRect.right() - x + 1) : width;
            window->setGeometry(x, areaRect.top(), qMax(1, w), areaRect.height());
        }
    } else {
        const int height = areaRect.height() / count;

        for (int i = 0; i < count; ++i) {
            QMdiSubWindow *window = windows.at(i);

            if (window->isMaximized())
                window->setWindowState(window->windowState() & ~Qt::WindowMaximized);

            const int y = areaRect.top() + i * height;
            const int h = (i == count - 1) ? (areaRect.bottom() - y + 1) : height;
            window->setGeometry(areaRect.left(), y, areaRect.width(), qMax(1, h));
        }
    }
}

} // namespace

// 构造函数：准备界面、对话框和信号连接
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this); // uic 生成的界面代码：MDI 区域、菜单、工具栏

    findDialog = new FindReplaceDialog(this); // 父对象是主窗口，Qt 负责释放

    connectActions(); // 把菜单/工具栏的点击连接到下面的槽函数

    // 切换当前文档时，刷新动作状态（例如撤销按钮是否可用）。
    connect(ui->mdiArea, &QMdiArea::subWindowActivated,
            this, &MainWindow::updateActions);

    // 查找对话框打开时，让它跟着当前文档切换。
    connect(ui->mdiArea, &QMdiArea::subWindowActivated, this, [this] {
        if (findDialog->isVisible()) {
            Document *doc = currentDocument();
            findDialog->setEditor(doc ? doc->editor() : nullptr);
        }
    });

    newFile();      // 启动时先创建一个空白文档
    updateActions();
}

MainWindow::~MainWindow()
{
    // 删除界面时会销毁 MDI 子窗口。先断开 mdiArea 的信号，
    // 避免析构过程中又回调 updateActions() 去访问正在销毁的控件。
    QObject::disconnect(ui->mdiArea, nullptr, this, nullptr);

    delete ui;
    ui = nullptr;
}

// ---------------------------- 文件操作 ----------------------------

// 新建文档：创建一个空白子窗口
void MainWindow::newFile()
{
    auto *doc = new Document(this);

    // 给新建文档一个编号，多个“未命名”窗口才能区分开。
    doc->setUntitledName(tr("未命名 %1").arg(++untitledCounter));

    addDocument(doc);
    doc->editor()->setFocus();
}

// 打开文档：文件对话框 -> 读取 -> 加入 MDI 区域
void MainWindow::openFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("打开文件"), QDir::homePath(),
        tr("文本文件 (*.txt *.md *.log);;所有文件 (*.*)"));

    if (filePath.isEmpty())
        return; // 用户取消

    auto *doc = new Document(this);

    // 读取失败时，这个子窗口还没有加入 MDI 区域，直接 delete 掉。
    if (!doc->openFile(filePath)) {
        delete doc;
        return;
    }

    addDocument(doc);
    doc->editor()->setFocus();
}

// 保存当前文档：具体分支在 Document::save() 里
void MainWindow::saveFile()
{
    Document *doc = currentDocument();
    if (doc)
        doc->save();
}

// ---------------------------- 编辑 ----------------------------

// 查找：把当前文档的编辑器交给查找对话框
void MainWindow::showFindDialog()
{
    Document *doc = currentDocument();
    if (!doc)
        return;

    findDialog->setEditor(doc->editor());
    findDialog->showFindMode();
}

// 替换
void MainWindow::showReplaceDialog()
{
    Document *doc = currentDocument();
    if (!doc)
        return;

    findDialog->setEditor(doc->editor());
    findDialog->showReplaceMode();
}

// ---------------------------- 格式 ----------------------------

// 字体：QFontDialog 负责选字体，具体应用在 Document 里
void MainWindow::chooseFont()
{
    if (Document *doc = currentDocument()) {
        doc->chooseFont();
        updateActions();
    }
}

// 粗体：动作是 checkable 的，参数 bold 就是点击后的勾选状态
void MainWindow::setBold(bool bold)
{
    if (Document *doc = currentDocument()) {
        doc->setBold(bold);
        updateActions();
    }
}

// 斜体
void MainWindow::setItalic(bool italic)
{
    if (Document *doc = currentDocument()) {
        doc->setItalic(italic);
        updateActions();
    }
}

// 下划线
void MainWindow::setUnderline(bool underline)
{
    if (Document *doc = currentDocument()) {
        doc->setUnderline(underline);
        updateActions();
    }
}

// 左对齐
void MainWindow::alignLeft()
{
    if (Document *doc = currentDocument()) {
        doc->setAlignment(Qt::AlignLeft);
        updateActions();
    }
}

// 居中
void MainWindow::alignCenter()
{
    if (Document *doc = currentDocument()) {
        doc->setAlignment(Qt::AlignHCenter);
        updateActions();
    }
}

// 右对齐
void MainWindow::alignRight()
{
    if (Document *doc = currentDocument()) {
        doc->setAlignment(Qt::AlignRight);
        updateActions();
    }
}

// ---------------------------- 窗口排列 ----------------------------

// 层叠：直接调用 Qt 原生函数
void MainWindow::cascadeWindows()
{
    ui->mdiArea->cascadeSubWindows();
}

// 水平平铺：使用上面自己写的区间划分算法
void MainWindow::tileWindowsHorizontally()
{
    tileMdiWindows(ui->mdiArea, Qt::Horizontal);
}

// 垂直平铺
void MainWindow::tileWindowsVertically()
{
    tileMdiWindows(ui->mdiArea, Qt::Vertical);
}

// ---------------------------- 状态同步 ----------------------------

// 根据当前文档的状态，刷新所有动作的可用性和勾选状态。
// 这个函数会在这些时候被调用：
//   切换文档、光标移动、格式变化、选区变化、撤销栈变化。
void MainWindow::updateActions()
{
    if (!ui)
        return;

    Document *doc = currentDocument();
    const bool hasDocument = (doc != nullptr);
    QTextEdit *editor = hasDocument ? doc->editor() : nullptr;

    // 文件/格式/查找类动作：没有文档时全部禁用。
    ui->actionSave->setEnabled(hasDocument);
    ui->actionFind->setEnabled(hasDocument);
    ui->actionReplace->setEnabled(hasDocument);

    ui->actionFont->setEnabled(hasDocument);
    ui->actionBold->setEnabled(hasDocument);
    ui->actionItalic->setEnabled(hasDocument);
    ui->actionUnderline->setEnabled(hasDocument);
    ui->actionAlignLeft->setEnabled(hasDocument);
    ui->actionAlignCenter->setEnabled(hasDocument);
    ui->actionAlignRight->setEnabled(hasDocument);

    // 撤销/重做：直接问 QTextDocument 的撤销栈，Qt 内部会维护。
    ui->actionUndo->setEnabled(hasDocument
                               && editor->document()->isUndoAvailable());
    ui->actionRedo->setEnabled(hasDocument
                               && editor->document()->isRedoAvailable());

    // 剪切/复制需要选区；粘贴需要剪贴板里有内容。
    const bool hasSelection = editor && editor->textCursor().hasSelection();
    ui->actionCut->setEnabled(hasSelection);
    ui->actionCopy->setEnabled(hasSelection);
    ui->actionPaste->setEnabled(editor && editor->canPaste());

    // 让工具栏的粗体/斜体/下划线/对齐按钮反映光标处的实际格式。
    if (editor) {
        ui->actionBold->setChecked(editor->fontWeight() >= QFont::Bold);
        ui->actionItalic->setChecked(editor->fontItalic());
        ui->actionUnderline->setChecked(editor->fontUnderline());

        const Qt::Alignment alignment = editor->alignment();
        ui->actionAlignLeft->setChecked(alignment.testFlag(Qt::AlignLeft));
        ui->actionAlignCenter->setChecked(alignment.testFlag(Qt::AlignHCenter));
        ui->actionAlignRight->setChecked(alignment.testFlag(Qt::AlignRight));
    } else {
        ui->actionBold->setChecked(false);
        ui->actionItalic->setChecked(false);
        ui->actionUnderline->setChecked(false);
        ui->actionAlignLeft->setChecked(false);
        ui->actionAlignCenter->setChecked(false);
        ui->actionAlignRight->setChecked(false);
    }
}

// ---------------------------- 文档管理 ----------------------------

// 当前活动子窗口就是当前文档。
// activeSubWindow() 返回 QMdiSubWindow*，用 qobject_cast 安全转成 Document*；
// 如果当前没有子窗口，返回 nullptr。
Document *MainWindow::currentDocument() const
{
    return qobject_cast<Document *>(ui->mdiArea->activeSubWindow());
}

// 把文档加入 MDI 区域
void MainWindow::addDocument(Document *doc)
{
    connectDocument(doc);              // 先连接信号，再显示窗口
    ui->mdiArea->addSubWindow(doc);    // Qt 接管子窗口的显示和位置管理
    doc->show();
    ui->mdiArea->setActiveSubWindow(doc);
}

// 连接一个文档发出的信号：
// 编辑器状态一变，就调用 updateActions() 刷新菜单/工具栏。
void MainWindow::connectDocument(Document *doc)
{
    QTextEdit *editor = doc->editor();

    // 光标移动、字符格式变化、选区变化。
    connect(editor, &QTextEdit::cursorPositionChanged,
            this, &MainWindow::updateActions);
    connect(editor, &QTextEdit::currentCharFormatChanged,
            this, &MainWindow::updateActions);
    connect(editor, &QTextEdit::copyAvailable,
            this, &MainWindow::updateActions);

    // 撤销栈变化：撤销/重做按钮是否可用。
    connect(editor->document(), &QTextDocument::undoAvailable,
            this, &MainWindow::updateActions);
    connect(editor->document(), &QTextDocument::redoAvailable,
            this, &MainWindow::updateActions);
}

// ---------------------------- 动作连接 ----------------------------

void MainWindow::connectActions()
{
    // 文件
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    // 编辑：撤销/重做/剪切/复制/粘贴直接调用 QTextEdit 的原生函数。
    connect(ui->actionUndo, &QAction::triggered, this, [this] {
        if (Document *doc = currentDocument())
            doc->editor()->undo();
    });
    connect(ui->actionRedo, &QAction::triggered, this, [this] {
        if (Document *doc = currentDocument())
            doc->editor()->redo();
    });
    connect(ui->actionCut, &QAction::triggered, this, [this] {
        if (Document *doc = currentDocument())
            doc->editor()->cut();
    });
    connect(ui->actionCopy, &QAction::triggered, this, [this] {
        if (Document *doc = currentDocument())
            doc->editor()->copy();
    });
    connect(ui->actionPaste, &QAction::triggered, this, [this] {
        if (Document *doc = currentDocument())
            doc->editor()->paste();
    });

    // 查找/替换
    connect(ui->actionFind, &QAction::triggered,
            this, &MainWindow::showFindDialog);
    connect(ui->actionReplace, &QAction::triggered,
            this, &MainWindow::showReplaceDialog);

    // 格式
    connect(ui->actionFont, &QAction::triggered, this, &MainWindow::chooseFont);
    connect(ui->actionBold, &QAction::triggered, this, &MainWindow::setBold);
    connect(ui->actionItalic, &QAction::triggered, this, &MainWindow::setItalic);
    connect(ui->actionUnderline, &QAction::triggered, this, &MainWindow::setUnderline);

    connect(ui->actionAlignLeft, &QAction::triggered, this, &MainWindow::alignLeft);
    connect(ui->actionAlignCenter, &QAction::triggered, this, &MainWindow::alignCenter);
    connect(ui->actionAlignRight, &QAction::triggered, this, &MainWindow::alignRight);

    // 三个对齐动作互斥：同一时间只能有一个被勾选。
    auto *alignmentGroup = new QActionGroup(this);
    alignmentGroup->setExclusive(true);
    alignmentGroup->addAction(ui->actionAlignLeft);
    alignmentGroup->addAction(ui->actionAlignCenter);
    alignmentGroup->addAction(ui->actionAlignRight);

    // 窗口排列
    connect(ui->actionCascade, &QAction::triggered,
            this, &MainWindow::cascadeWindows);
    connect(ui->actionTileHorizontal, &QAction::triggered,
            this, &MainWindow::tileWindowsHorizontally);
    connect(ui->actionTileVertical, &QAction::triggered,
            this, &MainWindow::tileWindowsVertically);
}
