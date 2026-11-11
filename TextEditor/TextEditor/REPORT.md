# 实验三 多文档文本编辑器

## 1 系统概述

本系统基于 Qt 6、C++ 与 Qt Widgets 实现一个多文档文本编辑器。主窗口采用 QMainWindow，中央工作区使用 QMdiArea；每个文档由 Document 类表示，继承 QMdiSubWindow，内部持有一个独立的 QTextEdit。因此多个文档可以同时打开，各自拥有独立的内容、光标和撤销栈。

系统支持新建、打开、保存文件，撤销/重做、剪切/复制/粘贴，字体、粗体、斜体、下划线与段落对齐，以及窗口层叠、水平平铺和垂直平铺等基本功能。查找替换由独立的 FindReplaceDialog 完成，是本项目的重点：对话框以非模态方式显示，提供“查找下一个”“替换”“全部替换”三个操作，并在状态标签中提示查找或替换结果。其余功能主要调用 Qt 已经提供的接口，本文只作简要说明。

## 2 系统设计

### 2.1 设计目标

本系统的主要设计目标如下：

1. 使用 QMdiArea 管理多个文档，每个 Document 拥有独立的 QTextEdit 和文件路径。
2. 完成文件新建、打开、保存，以及常用编辑、字符格式、段落对齐和窗口排列功能。
3. 实现非模态查找替换对话框，查找过程中不阻塞主窗口，允许用户切换文档。
4. 查找下一个：第一次从文档开头查找，之后从当前光标位置继续。
5. 替换当前：仅当当前选区与查找内容一致时替换，避免把无关文字误替换。
6. 全部替换：循环处理所有匹配，并把多次修改合并为一次撤销。
7. 主窗口只负责命令调度，文档操作封装在 Document，查找替换逻辑封装在 FindReplaceDialog。

### 2.2 设计分析与算法流程

**（1）总体结构**

程序由三个类构成。MainWindow 负责界面与调度：它通过 `ui->mdiArea` 管理子窗口，用 `currentDocument()` 取得当前活动文档，再把文件、格式、窗口等命令转发给 Document。Document 继承 QMdiSubWindow，内部保存一个 QTextEdit、当前文件路径 `currentFilePath` 和未命名显示名 `untitledName`，负责文件读写与格式设置。FindReplaceDialog 负责查找替换，通过 `QPointer<QTextEdit>` 指向当前文档的编辑区。菜单、工具栏和快捷键共享同一批 QAction，因此按钮状态只需要在 `updateActions()` 中维护一份。

```cpp
Document *MainWindow::currentDocument() const
{
    return qobject_cast<Document *>(ui->mdiArea->activeSubWindow());
}
```

当前没有文档时，`activeSubWindow()` 返回空指针，`qobject_cast` 的结果也是空指针，所有命令在使用文档前都会先判断。这样主窗口不需要额外保存“当前文档”成员，用户点击哪个子窗口，后续操作就作用于哪个文档。

**（2）非模态对话框与目标编辑器**

FindReplaceDialog 继承 QDialog，界面由两个 QLineEdit、一个 QLabel 和四个 QPushButton 组成。对话框使用 `show()` 显示而不是 `exec()`，因此是非模态的：查找框打开时，用户仍然可以切换或编辑主窗口中的文档。

对话框不直接保存某个文档的指针，而是用 `QPointer<QTextEdit> editorWidget` 保存当前编辑区。QPointer 在对应的 QTextEdit 被销毁后会自动变为空指针，避免文档关闭后对话框继续访问悬空对象。`MainWindow::showFindDialog()` 和 `showReplaceDialog()` 先取得当前 Document，再调用 `setEditor(doc->editor())` 把目标交给对话框；切换活动子窗口时，如果对话框可见，也会重新 `setEditor()`。`setEditor()` 还会把 `startFromBeginning` 置为 true，使新文档的下一次查找从开头开始。

```cpp
void FindReplaceDialog::setEditor(QTextEdit *editor)
{
    editorWidget = editor;
    startFromBeginning = true;   // 换了文档，下一次从新文档开头找
}
```

`showFindMode()` 和 `showReplaceMode()` 只负责显示、置顶和聚焦对应的输入框，不执行查找。查找和替换的入口分别由按钮的 `clicked` 信号以及输入框的 `returnPressed` 信号触发。

**（3）查找下一个**

`findNext()` 是查找功能的核心。它先检查 `editorWidget` 是否为空、查找内容是否为空；任一条件不满足时，只在状态标签中提示，不进行匹配。

查找起点由 `startFromBeginning` 决定：为 true 时新建一个指向文档开头的 QTextCursor；为 false 时使用编辑器当前光标。第一次查找从文档开头开始，之后 `setTextCursor(found)` 会把光标放在匹配文本的末尾，下一次查找自然从这里继续。

```cpp
QTextCursor start = startFromBeginning
    ? QTextCursor(editorWidget->document())
    : editorWidget->textCursor();
QTextCursor found = editorWidget->document()->find(text, start);
if (found.isNull()) {
    statusLabel->setText(tr("未找到“%1”").arg(text));
    return;
}
editorWidget->setTextCursor(found);
editorWidget->ensureCursorVisible();
startFromBeginning = false;
statusLabel->setText(tr("已找到匹配内容"));
```

匹配本身由 `QTextDocument::find()` 完成，使用 Qt 原生的纯文本匹配，默认不区分大小写。found 为空表示从起点到文档末尾都没有匹配；此时状态标签显示“未找到”，并保持 `startFromBeginning` 不变。找到匹配后，`setTextCursor(found)` 选中匹配文本，`ensureCursorVisible()` 让编辑区滚动到该位置，随后把 `startFromBeginning` 置为 false。

当前简化版本没有实现循环查找：查找到文档末尾后不会自动回到开头。这样逻辑更直接，也避免了“查找下一个”在唯一匹配处反复循环。若需要循环，只需在 found 为空时用指向文档开头的 cursor 再调用一次 `find()`。

查找内容输入框的 `textChanged` 信号会把 `startFromBeginning` 重新置为 true。因此用户修改查找词后，下一次查找重新从文档开头开始，而不是沿用旧关键词留下的光标位置。

**（4）替换当前**

`replace()` 先检查目标编辑器和查找内容，然后取当前光标 cursor。只有 `cursor.hasSelection()` 为真，且 `selectedText()` 与查找内容在不区分大小写的情况下相等时，才调用 `cursor.insertText(replaceEdit->text())` 覆盖当前选区，并把状态设为“已替换 1 处”。

```cpp
QTextCursor cursor = editorWidget->textCursor();
if (cursor.hasSelection()
    && cursor.selectedText().compare(findText, Qt::CaseInsensitive) == 0) {
    cursor.insertText(replaceEdit->text());
    editorWidget->setTextCursor(cursor);
    statusLabel->setText(tr("已替换 1 处"));
}
findNext();   // 替换完从当前光标继续找下一个
```

这个判断避免把光标处无关的选中文字误替换。比较使用 `Qt::CaseInsensitive`，与 `QTextDocument::find()` 默认不区分大小写的行为保持一致。无论是否执行替换，函数最后都会调用 `findNext()`，把光标移动到下一个匹配处，方便用户连续操作。单次替换只修改一处文本，因此它本身就是一次独立的撤销步骤。

**（5）全部替换**

`replaceAll()` 从文档开头建立一个 `searchCursor`，调用 `beginEditBlock()` 后循环查找。每找到一处，就调用 `found.insertText(replaceText)` 替换，然后执行 `searchCursor.setPosition(found.position())`，把下一次查找的起点放在替换文本之后。这样即使替换文本中包含查找词（例如把“文档”替换为“文档管理”），也不会在原地反复匹配造成死循环。

```cpp
QTextCursor searchCursor(document);
searchCursor.beginEditBlock();
int count = 0;
QTextCursor found = document->find(findText, searchCursor);
while (!found.isNull()) {
    found.insertText(replaceText);
    searchCursor.setPosition(found.position());
    found = document->find(findText, searchCursor);
    ++count;
}
searchCursor.endEditBlock();
```

循环结束后调用 `endEditBlock()`。`beginEditBlock()`/`endEditBlock()` 把循环中的所有修改合并成一个编辑块，因此用户按一次 Ctrl+Z 就能撤销全部替换，而不是逐处撤销。函数最后根据 count 显示“共替换 N 处”或“未找到”。

全部替换从文档开头一直处理到末尾，不需要额外的回绕逻辑；由于每次替换后都从 `found.position()` 继续，循环一定会向前推进。

**（6）边界情况与状态提示**

- 查找内容为空：三个操作都会提示“请输入要查找的内容”，并把焦点放回查找输入框。
- 没有目标编辑器：提示“当前没有可查找的文档”，不访问空指针。
- 查找到文档末尾：提示“未找到”，不自动回到开头；用户修改查找词或重新打开对话框后，才会从文档开头重新开始。
- 文档被关闭：QPointer 自动置空；切换文档：MainWindow 重新 setEditor()，对话框始终作用于当前活动文档。

**（7）其他功能简述**

文件操作集中在 Document：`openFile()` 用 QFile + QTextStream 按 UTF-8 读取，`save()` 对未命名文档弹出 QFileDialog、已有路径直接写回，`writeToFile()` 只保存纯文本。编辑命令撤销、重做、剪切、复制、粘贴直接调用 QTextEdit 的对应函数，MainWindow 根据 QTextDocument 的 `undoAvailable`/`redoAvailable` 和当前选区刷新 QAction 状态。格式命令中，字体由 QFontDialog 选择，粗体、斜体、下划线通过 QTextCharFormat 合并到选区，段落对齐通过 QTextBlockFormat::setAlignment() 与 `mergeBlockFormat()` 完成；`Document::applyCharFormat()` 统一保证只修改选中文字并更新后续输入格式。窗口层叠调用 `QMdiArea::cascadeSubWindows()`，水平/垂直平铺由 `tileMdiWindows()` 按 viewport 尺寸平分，并让最后一个窗口吸收整除余数。这些功能均以 Qt 现有接口为主，不再展开。

程序整体运行流程如图 1 所示，查找替换内部流程如图 2 所示。

![多文档文本编辑器整体运行流程](.picture/overall_flow.png)

*多文档文本编辑器整体运行流程*

![查找替换核心流程](picture/find_replace_flow.png)

*查找替换核心流程*

### 2.3 界面设计

主窗口界面由 Qt Designer 维护在 `mainwindow.ui` 中：中央是 QMdiArea，上方是菜单栏和文本工具栏，没有额外状态栏。菜单分为文件、编辑、格式、窗口四组；工具栏提供新建、打开、保存、撤销、重做、剪切、复制、粘贴、字体、粗体、斜体、下划线等常用命令。每个文档子窗口的标题显示文件名或“未命名 N”，多个文档可通过“窗口”菜单排列。

查找替换对话框采用两行表单布局：第一行是查找内容，第二行是替换内容；下方依次是“查找下一个”“替换”“全部替换”“关闭”四个按钮，最下方是状态标签。界面不提供区分大小写、全字匹配等选项，保持简化版的实现范围。

![程序主界面](picture/screenshot_main_crop.png)

*程序主界面*

## 3 系统实现与运行调试

### 3.1 系统实现

本项目的主要开发环境和技术如下：

| 项目 | 内容 |
| --- | --- |
| 编程语言 | C++17 |
| GUI 框架 | Qt 6.11.2（MinGW 64-bit） |
| 开发工具 | Qt Creator |
| 界面设计 | Qt Designer（mainwindow.ui） |
| 构建工具 | CMake |
| 核心模块 | Qt::Core、Qt::Widgets |
| 文本编辑 | QTextEdit、QTextDocument、QTextCursor |
| 文件读写 | QFile、QTextStream（UTF-8） |

程序主要构成：

- main.cpp：创建 QApplication，加载 Qt 中文翻译，显示 MainWindow。
- mainwindow.h/.cpp/.ui：主窗口、菜单与工具栏、QAction 连接、文档调度和窗口排列。
- document.h/.cpp：单个文档子窗口，文件读写、标题、字符与段落格式。
- findreplacedialog.h/.cpp：非模态查找替换对话框，以及查找、替换、全部替换算法。
- CMakeLists.txt：配置 Qt6 Core/Widgets 依赖和可执行文件。

程序结构如图 3 所示。

![程序结构图](picture/program_structure.png)

*程序结构图*

主要信号槽连接：

- 按钮 clicked → findNext() / replace() / replaceAll()；输入框 returnPressed → 查找或替换。
- 查找框 textChanged → startFromBeginning = true，下一次查找重新从文档开头开始。
- QMdiArea::subWindowActivated → updateActions()；查找框可见时同时 setEditor() 当前文档。
- QTextDocument::undoAvailable/redoAvailable、QTextEdit::copyAvailable → updateActions() 刷新按钮状态。

### 3.2 运行调试

为检验系统功能，设计以下测试用例。

**测试 1：启动与多文档**

操作：启动程序，按两次 Ctrl+N，分别输入不同内容，选择“窗口 → 垂直平铺”。

结果：主窗口显示“未命名 1/2/3”，每个文档内容独立；垂直平铺后三个子窗口上下排列。

![多个文档垂直平铺](picture/screenshot_multidoc.png)

*多个文档垂直平铺*

**测试 2：查找下一个**

操作：在文档中输入多个“文档”，按 Ctrl+F 打开对话框，输入“文档”，点击“查找下一个”。

结果：第一次从文档开头选中第一个“文档”；再次点击选中下一个；查找到末尾后提示“未找到”，不会回到开头。多次点击时，光标始终从上一处匹配的末尾继续。

![查找替换对话框](picture/screenshot_find_crop.png)

*查找替换对话框*

**测试 3：修改查找词**

操作：把查找内容改为“编辑器”，再次点击“查找下一个”。

结果：textChanged 将 startFromBeginning 置为 true，查找从文档开头重新开始，而不是沿用旧关键词的位置。

**测试 4：替换当前**

操作：先查找使“文档”处于选中状态，在“替换为”输入“文件”，点击“替换”。

结果：当前“文档”变为“文件”，状态显示“已替换 1 处”，光标移动到下一个匹配。若当前选区不是查找内容，则不替换，只移动到下一个匹配，避免误替换。

**测试 5：全部替换**

操作：查找“文档”、替换为“文档管理”，点击“全部替换”。

结果：所有“文档”替换为“文档管理”，状态显示替换处数；按一次 Ctrl+Z 全部恢复，说明多次修改被合并成一个编辑块。替换文本包含查找词时也不会死循环。

**测试 6：空查找内容与无文档**

操作：清空查找内容后点击“查找下一个”；关闭所有文档后再次打开查找框。

结果：分别提示“请输入要查找的内容”和“当前没有可查找的文档”，程序不崩溃。

**测试 7：切换文档**

操作：保持查找框打开，切换到另一个文档，再点击“查找下一个”。

结果：对话框自动 setEditor() 到新文档，查找从新文档开头开始；关闭原文档后 QPointer 自动置空，不会访问已销毁的编辑区。

**测试 8：其他功能**

操作：测试保存并重新打开 UTF-8 中文文件、粗体/斜体/下划线、段落对齐、层叠与平铺。

结果：文件内容正常，格式只作用于选区，窗口排列正确；字体等格式不会写入纯文本文件。

以上用例覆盖了查找替换的三条主路径和主要边界情况。运行中未出现崩溃或断言；查找对话框非模态，切换文档后仍能正常工作。

## 4 系统扩展

### 4.1 多文档隔离

每个 Document 持有独立的 QTextEdit 和 QTextDocument，内容、光标、撤销栈互不影响；主窗口不保存文本，只通过 `activeSubWindow()` 取得当前文档。这样新增文档只需创建一个 Document，不需要改动文件、格式等具体逻辑。

### 4.2 非模态查找替换

FindReplaceDialog 独立成类，使用 `QPointer` 安全持有目标编辑器；对话框打开时仍可操作主窗口，切换文档后自动更新目标。查找界面和算法不放在 MainWindow 中，减少了主窗口的职责。

### 4.3 查找策略

首次从文档开头、之后从光标继续；替换只在选区匹配时执行；全部替换用 `beginEditBlock()`/`endEditBlock()` 合并撤销，并通过 `setPosition()` 跳过已替换文本，避免替换文本包含查找词时原地循环。这三条策略分别对应连续查找、单次替换和批量替换。

### 4.4 窗口平铺与代码组织

层叠调用 Qt 原生的 `cascadeSubWindows()`；水平/垂直平铺自行计算 viewport 几何，最后一个窗口吸收整除余数。界面集中在 `mainwindow.ui`，主窗口只做调度，Document 封装文档，FindReplaceDialog 封装查找替换；QAction 同时驱动菜单和工具栏，`updateActions()` 统一刷新状态。

## 5 总结

本项目以 QMdiArea + Document + QTextEdit 为核心，实现了一个功能完整但结构简洁的多文档文本编辑器。文件、编辑、格式、窗口等基础功能主要复用 Qt 提供的接口，代码量集中在文档封装和界面调度上。

查找替换是本项目的设计重点。FindReplaceDialog 采用非模态对话框和 QPointer 目标编辑器；`findNext()` 通过 startFromBeginning 控制首次/后续查找；`replace()` 先比较选区再替换，避免误操作；`replaceAll()` 用 `beginEditBlock()`/`endEditBlock()` 合并撤销，并在替换后从 `found.position()` 继续，避免替换文本包含查找词时死循环。这三个函数覆盖了查找替换的主要使用场景。

当前版本仍有改进空间：查找不支持循环、区分大小写、全字匹配和正则表达式；文档以 UTF-8 纯文本保存，字符格式不会保留；关闭文档前没有未保存提示。后续可以增加 `FindCaseSensitively`/`FindWholeWords` 选项、循环查找、QSaveFile 原子写入和关闭确认等功能。

通过本次设计，掌握了 QMdiArea 的多文档管理、QTextCursor 的选区与格式操作、QFile/QTextStream 的文本读写，以及用信号槽把对话框与当前文档连接起来的方法。查找替换的实现也加深了对 Qt 原生 `find()`、编辑块和 QPointer 生命周期管理的理解。
