#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include "xlsxdocument.h"

class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Excel Processor - 环境测试");
        setMinimumSize(500, 300);

        auto* central = new QWidget(this);
        auto* layout  = new QVBoxLayout(central);

        label = new QLabel("点击按钮选择一个 xlsx 文件", this);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);

        auto* btn = new QPushButton("打开 xlsx 文件", this);

        connect(btn, &QPushButton::clicked, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, "选择Excel文件", "", "Excel Files (*.xlsx)");
            if (path.isEmpty()) return;

            QXlsx::Document xlsx(path);
            QString result = "读取结果：\n";
            for (int row = 1; row <= 5; ++row) {
                for (int col = 1; col <= 5; ++col) {
                    auto cell = xlsx.cellAt(row, col);
                    if (cell) {
                        result += QString("[%1,%2]=%3  ")
                                  .arg(row).arg(col)
                                  .arg(cell->value().toString());
                    }
                }
                result += "\n";
            }
            label->setText(result);
            QMessageBox::information(this, "成功", "QXlsx 读取成功！");
        });

        layout->addWidget(label);
        layout->addWidget(btn);
        setCentralWidget(central);
    }

private:
    QLabel* label;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}