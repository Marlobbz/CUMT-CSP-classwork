#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QMdiSubWindow>
#include <QTextCharFormat>

class QTextEdit;

// 一个 Document 就是一个 MDI 子窗口，内部放一个独立的 QTextEdit。
// 文件路径、打开/保存、字体格式都封装在这个类里，主窗口只负责调度。
class Document : public QMdiSubWindow
{
    Q_OBJECT

public:
    explicit Document(QWidget *parent = nullptr);

    // 文件操作
    bool openFile(const QString &filePath); // 从磁盘读取文件到编辑器
    bool save();                            // 保存当前文档（未命名时会弹保存对话框）
    QString displayName() const;            // 标题栏显示的名字
    bool isUntitled() const { return currentFilePath.isEmpty(); } // 是否是新建文档
    void setUntitledName(const QString &name); // 设置“未命名 1”这样的显示名

    QTextEdit *editor() const { return textEdit; } // 返回本文档的编辑控件

    // 格式设置：都只作用于选区；没有选区时作用于接下来输入的文字
    bool chooseFont();                            // 选择字体
    void setBold(bool bold);                      // 粗体
    void setItalic(bool italic);                  // 斜体
    void setUnderline(bool underline);            // 下划线
    void setAlignment(Qt::Alignment alignment);   // 段落对齐

private:
    bool writeToFile(const QString &filePath);    // 把编辑器里的纯文本写入磁盘
    void applyCharFormat(const QTextCharFormat &format); // 把字符格式合并到选区/当前输入
    void updateTitle();                           // 根据文件名刷新窗口标题

    QTextEdit *textEdit = nullptr; // 本文档的编辑控件，由 setWidget() 托管
    QString currentFilePath;       // 空：新建文档；非空：已打开或已保存的路径
    QString untitledName;          // 新建文档的显示名，例如“未命名 1”
};

#endif // DOCUMENT_H
