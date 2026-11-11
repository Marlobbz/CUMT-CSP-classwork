#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Document;
class FindReplaceDialog;

// 主窗口：创建菜单/工具栏动作，把用户操作转发给“当前活动文档”。
// 它本身不保存文本，文本都在各个 Document 里。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 文件
    void newFile();   // 新建文档
    void openFile();  // 打开文件
    void saveFile();  // 保存当前文档

    // 编辑
    void showFindDialog();    // 显示查找对话框
    void showReplaceDialog(); // 显示替换对话框

    // 格式
    void chooseFont();          // 选择字体
    void setBold(bool bold);    // 粗体
    void setItalic(bool italic);// 斜体
    void setUnderline(bool underline); // 下划线
    void alignLeft();           // 左对齐
    void alignCenter();         // 居中
    void alignRight();          // 右对齐

    // 窗口排列
    void cascadeWindows();          // 层叠
    void tileWindowsHorizontally(); // 水平平铺
    void tileWindowsVertically();   // 垂直平铺

    // 根据当前文档状态，刷新菜单/工具栏的可用和勾选状态
    void updateActions();

private:
    Document *currentDocument() const;      // 获取当前活动文档
    void addDocument(Document *doc);        // 把文档加入 MDI 区域
    void connectDocument(Document *doc);    // 连接文档发出的信号
    void connectActions();                  // 连接菜单/工具栏动作

    Ui::MainWindow *ui;
    FindReplaceDialog *findDialog = nullptr; // 查找替换对话框
    int untitledCounter = 0;                 // 未命名文档编号
};

#endif // MAINWINDOW_H
