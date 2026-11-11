#ifndef FINDREPLACEDIALOG_H
#define FINDREPLACEDIALOG_H

#include <QDialog>
#include <QPointer>

class QLabel;
class QLineEdit;
class QTextEdit;

// 查找替换对话框：非模态，始终操作当前活动文档的 QTextEdit。
class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindReplaceDialog(QWidget *parent = nullptr);

    void setEditor(QTextEdit *editor); // 指定要操作的编辑器
    void showFindMode();               // 显示对话框并聚焦“查找内容”
    void showReplaceMode();            // 显示对话框并聚焦“替换为”

private slots:
    void findNext();   // 查找下一个（第一次从开头，之后从光标后继续）
    void replace();    // 替换当前匹配
    void replaceAll(); // 全部替换

private:
    QLineEdit *findEdit = nullptr;    // 查找内容输入框
    QLineEdit *replaceEdit = nullptr; // 替换内容输入框
    QLabel *statusLabel = nullptr;    // 显示“找到/未找到/替换了几处”

    // QPointer：编辑器被销毁后会自动变成空指针，避免访问野指针。
    QPointer<QTextEdit> editorWidget;

    // true：下一次查找从文档开头开始；
    // false：从编辑器当前光标位置开始（光标在上一处匹配的末尾）。
    bool startFromBeginning = true;
};

#endif // FINDREPLACEDIALOG_H
