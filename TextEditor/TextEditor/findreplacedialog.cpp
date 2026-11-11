#include "findreplacedialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

// 构造函数：用布局管理器搭出“两行输入框 + 一排按钮 + 状态提示”
FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("查找和替换"));
    setMinimumWidth(400);

    findEdit = new QLineEdit(this);
    findEdit->setPlaceholderText(tr("输入要查找的内容"));

    replaceEdit = new QLineEdit(this);
    replaceEdit->setPlaceholderText(tr("输入替换后的内容"));

    statusLabel = new QLabel(this);
    statusLabel->setMinimumHeight(24);

    auto *findButton = new QPushButton(tr("查找下一个"), this);
    auto *replaceButton = new QPushButton(tr("替换"), this);
    auto *replaceAllButton = new QPushButton(tr("全部替换"), this);
    auto *closeButton = new QPushButton(tr("关闭"), this);

    // 第一行：查找内容；第二行：替换内容。
    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("查找内容："), findEdit);
    formLayout->addRow(tr("替换为："), replaceEdit);

    // 按钮排成一行，关闭按钮放最右边。
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(findButton);
    buttonLayout->addWidget(replaceButton);
    buttonLayout->addWidget(replaceAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(statusLabel);

    // 信号槽：按钮点击 -> 对应的处理函数。
    connect(findButton, &QPushButton::clicked,
            this, &FindReplaceDialog::findNext);
    connect(replaceButton, &QPushButton::clicked,
            this, &FindReplaceDialog::replace);
    connect(replaceAllButton, &QPushButton::clicked,
            this, &FindReplaceDialog::replaceAll);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::close);

    // 在输入框里按回车也执行对应操作。
    connect(findEdit, &QLineEdit::returnPressed,
            this, &FindReplaceDialog::findNext);

    // 查找内容一改，下一次查找就重新从文档开头开始。
    connect(findEdit, &QLineEdit::textChanged, this, [this] {
        startFromBeginning = true;
    });
    connect(replaceEdit, &QLineEdit::returnPressed,
            this, &FindReplaceDialog::replace);
}

void FindReplaceDialog::setEditor(QTextEdit *editor)
{
    editorWidget = editor;
    startFromBeginning = true; // 换了文档，下一次查找从新文档开头开始

    if (!editorWidget)
        statusLabel->clear();
}

void FindReplaceDialog::showFindMode()
{
    statusLabel->clear();
    show();         // 非模态显示：对话框开着时仍可操作主窗口
    raise();        // 放到最前面
    activateWindow(); // 让对话框获得焦点

    findEdit->setFocus();
    findEdit->selectAll();
}

void FindReplaceDialog::showReplaceMode()
{
    statusLabel->clear();
    show();
    raise();
    activateWindow();

    replaceEdit->setFocus();
    replaceEdit->selectAll();
}

// 查找下一个：
// 第一次从文档开头找；找到后光标停在匹配文字末尾，
// 下一次就从光标位置继续往后找。不回到开头。
void FindReplaceDialog::findNext()
{
    if (!editorWidget) {
        statusLabel->setText(tr("当前没有可查找的文档"));
        return;
    }

    const QString text = findEdit->text();
    if (text.isEmpty()) {
        statusLabel->setText(tr("请输入要查找的内容"));
        findEdit->setFocus();
        return;
    }

    // 决定这次从哪里开始找：
    // - startFromBeginning 为 true：新建一个指向文档开头的 cursor；
    // - 否则：使用编辑器当前光标（它在上一次匹配的末尾）。
    QTextCursor start = startFromBeginning
                            ? QTextCursor(editorWidget->document())
                            : editorWidget->textCursor();

    // QTextDocument::find() 是 Qt 原生函数，匹配本身由它完成。
    QTextCursor found = editorWidget->document()->find(text, start);

    if (found.isNull()) {
        statusLabel->setText(tr("未找到“%1”").arg(text));
        return;
    }

    // 选中匹配文字。setTextCursor() 后光标位置在匹配内容的末尾，
    // 下一次 find() 就会从这里往后找，相当于“查找下一个”。
    editorWidget->setTextCursor(found);
    editorWidget->ensureCursorVisible();

    // 已经找到过一处，后续查找不再从文档开头开始。
    startFromBeginning = false;
    statusLabel->setText(tr("已找到匹配内容"));
}

// 替换当前匹配：只有当前选区正好等于“查找内容”时才替换
void FindReplaceDialog::replace()
{
    if (!editorWidget) {
        statusLabel->setText(tr("当前没有可查找的文档"));
        return;
    }

    const QString findText = findEdit->text();
    if (findText.isEmpty()) {
        statusLabel->setText(tr("请输入要查找的内容"));
        findEdit->setFocus();
        return;
    }

    QTextCursor cursor = editorWidget->textCursor();

    // 判断当前选中的文字是不是要查找的内容，避免误替换。
    // find() 默认不区分大小写，这里也按不区分大小写比较。
    if (cursor.hasSelection()
        && cursor.selectedText().compare(findText, Qt::CaseInsensitive) == 0) {

        // 用替换文本覆盖当前选区。
        cursor.insertText(replaceEdit->text());
        editorWidget->setTextCursor(cursor);
        statusLabel->setText(tr("已替换 1 处"));
    }

    // 替换完从当前光标位置继续找下一个。
    findNext();
}

// 全部替换：循环查找，直到文档末尾
void FindReplaceDialog::replaceAll()
{
    if (!editorWidget) {
        statusLabel->setText(tr("当前没有可查找的文档"));
        return;
    }

    const QString findText = findEdit->text();
    if (findText.isEmpty()) {
        statusLabel->setText(tr("请输入要查找的内容"));
        findEdit->setFocus();
        return;
    }

    const QString replaceText = replaceEdit->text();
    QTextDocument *document = editorWidget->document();

    // searchCursor 表示“下一次从哪里开始找”。
    QTextCursor searchCursor(document);

    // beginEditBlock() / endEditBlock() 把多次替换合并成一次撤销操作：
    // 用户按一次 Ctrl+Z 就能全部撤回，而不是撤一次只回退一处。
    searchCursor.beginEditBlock();

    int count = 0;
    QTextCursor found = document->find(findText, searchCursor);

    while (!found.isNull()) {
        // 替换当前匹配。
        found.insertText(replaceText);

        // 关键：从替换结果之后继续查找。
        // 如果替换文本里又包含查找词，也不会在这里原地死循环。
        searchCursor.setPosition(found.position());
        found = document->find(findText, searchCursor);

        ++count;
    }

    searchCursor.endEditBlock();

    if (count == 0)
        statusLabel->setText(tr("未找到“%1”").arg(findText));
    else
        statusLabel->setText(tr("共替换 %1 处").arg(count));
}
