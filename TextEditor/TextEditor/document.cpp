#include "document.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDialog>
#include <QMessageBox>
#include <QStringConverter>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextStream>

// 构造函数：搭建“子窗口 + 文本编辑器”的结构
Document::Document(QWidget *parent)
    : QMdiSubWindow(parent)
    , untitledName(tr("未命名"))
{
    // 每个文档都 new 一个自己的 QTextEdit，所以多个文档的内容互不影响。
    textEdit = new QTextEdit;

    // setWidget() 会把 textEdit 设为子窗口的内容控件，
    // 同时把 textEdit 的生命周期交给这个子窗口管理。
    setWidget(textEdit);

    // 用户点窗口右上角的关闭按钮后，Qt 会自动 delete 这个子窗口。
    setAttribute(Qt::WA_DeleteOnClose);

    resize(720, 520);
    updateTitle();
}

// ---------------------------- 文件操作 ----------------------------

// 打开文件：读取磁盘上的文本，放进 QTextEdit
bool Document::openFile(const QString &filePath)
{
    QFile file(filePath);

    // 以“只读 + 文本模式”打开。文本模式会自动处理换行符差异。
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法打开文件：\n%1\n\n原因：%2")
                                 .arg(QDir::toNativeSeparators(filePath),
                                      file.errorString()));
        return false;
    }

    // QTextStream 负责把字节流按指定编码转换成 QString。
    // 统一使用 UTF-8，中文才不会乱码。
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    const QString text = in.readAll();

    // 读取过程中如果出错（例如文件损坏），提示并放弃打开。
    if (in.status() != QTextStream::Ok) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("读取文件时发生错误：\n%1").arg(file.errorString()));
        return false;
    }
    file.close();

    // setPlainText() 会把整个文档内容替换成读到的纯文本。
    textEdit->setPlainText(text);

    // 光标回到文档开头，方便用户立即阅读或编辑。
    textEdit->moveCursor(QTextCursor::Start);

    // 记住文件路径。这个成员就是“文档标识”：
    // 为空表示新建文档，非空表示已经打开或保存过的文档。
    currentFilePath = filePath;
    updateTitle();
    return true;
}

// 保存：新建文档先选路径，已有路径的文档直接覆盖
bool Document::save()
{
    // 文档标识为“新建”：没有路径，必须让用户选择保存位置。
    if (isUntitled()) {
        QString filePath = QFileDialog::getSaveFileName(
            this, tr("保存"),
            QDir::homePath() + QLatin1Char('/') + untitledName + QStringLiteral(".txt"),
            tr("文本文件 (*.txt);;所有文件 (*.*)"));

        if (filePath.isEmpty())
            return false; // 用户点了取消

        // 用户没有写扩展名时，自动补上 .txt。
        if (QFileInfo(filePath).suffix().isEmpty())
            filePath += QStringLiteral(".txt");

        if (!writeToFile(filePath))
            return false;

        // 保存成功后才记录路径，标题从“未命名 1”变成真实文件名。
        currentFilePath = filePath;
        updateTitle();
        return true;
    }

    // 已打开或已保存过的文档：直接写回原路径，不再弹对话框。
    return writeToFile(currentFilePath);
}

// 真正写磁盘的函数
bool Document::writeToFile(const QString &filePath)
{
    QFile file(filePath);

    // Truncate：覆盖原文件，而不是在末尾追加。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("无法写入文件：\n%1\n\n原因：%2")
                                 .arg(QDir::toNativeSeparators(filePath),
                                      file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 本项目是纯文本编辑器，只保存文字内容，不保存字体、粗体等格式。
    out << textEdit->toPlainText();
    out.flush();

    // 检查写入是否成功：流状态和文件错误码都要看。
    if (out.status() != QTextStream::Ok || file.error() != QFileDevice::NoError) {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("写入文件时发生错误：\n%1").arg(file.errorString()));
        file.close();
        return false;
    }

    file.close();
    return true;
}

// 标题栏显示的名字：有路径就用文件名，否则用“未命名 N”
QString Document::displayName() const
{
    if (!currentFilePath.isEmpty())
        return QFileInfo(currentFilePath).fileName();
    return untitledName;
}

void Document::setUntitledName(const QString &name)
{
    untitledName = name;
    updateTitle();
}

void Document::updateTitle()
{
    setWindowTitle(displayName());
}

// ---------------------------- 格式设置 ----------------------------

// 所有字符格式（字体、粗体、斜体、下划线）最终都走这个函数。
void Document::applyCharFormat(const QTextCharFormat &format)
{
    QTextCursor cursor = textEdit->textCursor();

    // 有选区：mergeCharFormat() 把格式合并到选中的文字上。
    // merge 的意思是“只改 format 里设置过的属性”，
    // 例如只设置粗体，就不会把字号、字体一起改掉。
    if (cursor.hasSelection())
        cursor.mergeCharFormat(format);

    // 同时更新“当前输入格式”：
    // - 没有选区时，接下来输入的文字会使用这个格式；
    // - 有选区时，这行让编辑器记住该格式，取消选中后继续输入也保持一致。
    textEdit->mergeCurrentCharFormat(format);

    // 操作完把焦点还给编辑器，方便继续输入。
    textEdit->setFocus();
}

// 选择字体：QFontDialog 是 Qt 原生对话框
bool Document::chooseFont()
{
    bool ok = false;

    // 以光标处当前的字体作为对话框的初始值。
    const QFont selectedFont = QFontDialog::getFont(
        &ok, textEdit->currentFont(), this, tr("选择字体"));

    if (!ok)
        return false; // 用户取消

    // QTextCharFormat::setFont() 会一次带入字体族、字号、粗体、斜体等属性。
    QTextCharFormat format;
    format.setFont(selectedFont);
    applyCharFormat(format);
    return true;
}

// 粗体：设置字重（Bold / Normal），再合并到选区
void Document::setBold(bool bold)
{
    QTextCharFormat format;
    format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    applyCharFormat(format);
}

// 斜体
void Document::setItalic(bool italic)
{
    QTextCharFormat format;
    format.setFontItalic(italic);
    applyCharFormat(format);
}

// 下划线
void Document::setUnderline(bool underline)
{
    QTextCharFormat format;
    format.setFontUnderline(underline);
    applyCharFormat(format);
}

// 段落对齐：注意这是“块格式”，作用对象是段落而不是单个字符
void Document::setAlignment(Qt::Alignment alignment)
{
    QTextCursor cursor = textEdit->textCursor();

    QTextBlockFormat format;
    format.setAlignment(alignment);

    // 有选区：作用于选区覆盖的所有段落；
    // 没有选区：只作用于光标所在的那一段。
    cursor.mergeBlockFormat(format);
    textEdit->setFocus();
}
