#include "modulegen.h"

#include "../../common/string-utils/stringutils.h"
#include <QFileDialog>

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>

REGISTER_DYNAMICOBJECT(ModuleGen);

ModuleGen::ModuleGen() : QWidget(nullptr), DynamicObjectBase()
{

    QVBoxLayout * layout = new QVBoxLayout(this);

    QHBoxLayout * dirSelectLayout = new QHBoxLayout;
    dirSelectLayout->setAlignment(Qt::AlignLeft);

    QPushButton * selectDirButton = new QPushButton("模块所在目录");


    selectDirButton->setFixedSize(100, 30);

    dirSelectLayout->addWidget(selectDirButton);

    QLineEdit * dirLineEdit = new QLineEdit;
    dirLineEdit->setFixedSize(600, 30);
    dirSelectLayout->addWidget(dirLineEdit);

    layout->addLayout(dirSelectLayout);



    QHBoxLayout * classNameInputLayout = new QHBoxLayout;

    QLabel * classNameLabel = new QLabel("请输入类名");
    classNameLabel->setFixedSize(100, 30);
    classNameInputLayout->addWidget(classNameLabel);

    QLineEdit * classNameEdit = new QLineEdit;

    classNameEdit->setFixedSize(600, 30);


    classNameInputLayout->addWidget(classNameEdit);

    layout->addLayout(classNameInputLayout);
    classNameInputLayout->setAlignment(Qt::AlignLeft);




    QHBoxLayout * titleInputLayout = new QHBoxLayout;

    QLabel * titleLabel = new QLabel("请输入标题");
    titleLabel->setFixedSize(100, 30);
    titleInputLayout->addWidget(titleLabel);

    QLineEdit * titleEdit = new QLineEdit;

    titleEdit->setFixedSize(600, 30);


    titleInputLayout->addWidget(titleEdit);

    layout->addLayout(titleInputLayout);
    titleInputLayout->setAlignment(Qt::AlignLeft);


    connect(selectDirButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));


    layout->setAlignment(Qt::AlignTop);



    QPushButton * genButton = new QPushButton("生成");

    genButton->setFixedSize(50, 30);

    connect(genButton, SIGNAL(clicked()), this, SLOT(gen()));

    this->layout()->addWidget(genButton);

    this->dirLineEdit = dirLineEdit;
    this->classNameEdit = classNameEdit;
    this->titleEdit = titleEdit;
    this->setFixedHeight(300);


    this->setLayout(layout);
}

void ModuleGen::gen() {

    QString projectDir = this->dirLineEdit->text();

    QString basePath = projectDir;
    QString fileName = "modules";

    QDir baseDir(basePath);
    QString modulePath = baseDir.filePath(fileName);

    QString className = this->classNameEdit->text();

    QString title = this->titleEdit->text();

    if (modulePath.isEmpty()) {
        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("模块所在目录不存在");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }

    if (className.isEmpty()) {
        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("类名为空");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }



    if (title.isEmpty()) {
        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("标题为空");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }


    QDir directory(modulePath);

    if (directory.exists()) {
        qDebug() << "路径存在且是目录";
    } else {
        qDebug() << "路径不存在或不是目录";

        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("模块所在目录不是有效目录");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;

    }


    QString hyphenClassName = camelCaseToHyphen(className);


    QDir dir(modulePath);
    QString filePath = dir.filePath(hyphenClassName);

    QDir classDir(filePath);


    if (classDir.exists()) {

        qDebug() << "路径不存在或不是目录";

        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("模块已经存在了");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }

    if (!classDir.mkpath(filePath)) {

        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("模块目录创建失败");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }

/////////////////////////////////////////////////////////////////////////////////////////


    QString headerDefine = className.toUpper() + "_H";

    QString headerFilePath = classDir.filePath(className.toLower() + ".h"); // 替换为你要创建的文件路径

    // 创建文件对象
    QFile headerFile(headerFilePath);

    if (headerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 打开文件成功

        // 创建文本流
        QTextStream out(&headerFile);


        QString headerText = R"(
#ifndef %1
#define %1


#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#include <QWidget>

#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>

#include "../../common/dynamicobjectbase.h"


class %2 : public QWidget, public DynamicObjectBase {

    %3
public:
    explicit %2();

private:


public slots:
};

#endif // %1)";

        QString formattedHeaderText = headerText.arg(headerDefine, className, "Q_OBJECT");

        // 写入内容
        out << formattedHeaderText << "\n";

        // 关闭文件
        headerFile.close();

        qDebug() << "文件创建并写入成功：" << filePath;
    } else {
        qDebug() << "无法创建文件：" << filePath;
    }


    QString sourceFilePath = classDir.filePath(className.toLower() + ".cpp"); // 替换为你要创建的文件路径

    // 创建文件对象
    QFile sourceFile(sourceFilePath);

    if (sourceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 打开文件成功

        // 创建文本流
        QTextStream out(&sourceFile);


        QString sourceText = R"(#include "%1.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(%2);

%2::%2() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    label->setText("待实现.....");

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}
)";
        QString formattedSourceText = sourceText.arg(className.toLower(), className);

        // 写入内容
        out << formattedSourceText << "\n";

        // 关闭文件
        sourceFile.close();

        qDebug() << "文件创建并写入成功：" << sourceFilePath;
    } else {
        qDebug() << "无法创建文件：" << sourceFilePath;
    }

/////////////////////////////////////////////////////////////////////////////////////


    QString moduleMetaData = baseDir.filePath(QString("tool-list") + QDir::separator() + QString("module-meta-data.h"));

    QString moduleMetaFilePath = moduleMetaData; // 文件名
    QString textToAppend = QString(R"(
{"%1", "%2", "%3"},
)").arg("xxx", title, className); // 要追加的文本

    QFile moduleMetaFile(moduleMetaFilePath);

    // 打开文件以追加数据（如果文件不存在则创建）
    if (moduleMetaFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&moduleMetaFile);
        out << textToAppend;
        moduleMetaFile.close();
        qDebug() << "Data appended to file.";
    } else {
        qDebug() << "Failed to open or create the file.";
    }
}

void ModuleGen::onButtonClicked() {
    // 创建一个目录选择对话框
    QString directoryPath = QFileDialog::getExistingDirectory(nullptr, "选择目录", "", QFileDialog::ShowDirsOnly);

    this->dirLineEdit->setText(directoryPath);


    if (!directoryPath.isEmpty()) {
        // 用户选择了目录
        qDebug() << "选择的目录路径：" << directoryPath;
    } else {
        // 用户取消了选择
        qDebug() << "未选择目录";
    }
}
