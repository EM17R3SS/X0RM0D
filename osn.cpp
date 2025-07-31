#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QApplication>
#include <QDir>
#include <QFileInfoList>
#include <QFile>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTranslator>
#include <QLibraryInfo>

class Window : public QWidget
{
    Q_OBJECT

public:
    Window(QWidget *parent = nullptr) : QWidget(parent)
    {
        setupUI();
        connectSignals();
    }

private slots:
    void onStartClicked();
    void onStopClicked();
    void onBrowseClicked();
    void onTimeout();
    void onSelectFileClicked();
    void onInputPathBrowseClicked();

private:
    bool isProcessing = false;
    QFileInfoList fileList;
    int currentFileIndex = 0;

    QFutureWatcher<void> fileProcessingWatcher;

    QLineEdit *inputFileMask;
    QLineEdit *inputPathEdit;
    QLineEdit *resFileSavePath;
    QLineEdit *intervalEdit;
    QLineEdit *xorValueEdit;

    QCheckBox *checkDeleteInputFiles;
    QCheckBox *overwriteFile;
    QCheckBox *timerWorkCheck;

    QPushButton *browsePathButton;
    QPushButton *inputPathButton;
    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *selectFileButton;

    QProgressBar *progressBar;
    QLabel *statusLabel;
    QTimer *timer;

    void setupUI()
    {
        QLabel *inputPathLabel = new QLabel("Dir with src:", this);
        inputPathEdit = new QLineEdit(QDir::currentPath(), this);
        inputPathButton = new QPushButton("Browse", this);

        QLabel *maskLabel = new QLabel("Mask (example .txt):", this);
        inputFileMask = new QLineEdit("*.*", this);
        selectFileButton = new QPushButton("Choose current File", this);

        QLabel *deleteLabel = new QLabel("Delete:", this);
        checkDeleteInputFiles = new QCheckBox(this);

        QLabel *pathLabel = new QLabel("Dir for save:", this);
        resFileSavePath = new QLineEdit(QDir::currentPath(), this);
        browsePathButton = new QPushButton("Browse", this);

        QLabel *actionLabel = new QLabel("Overwrite:", this);
        overwriteFile = new QCheckBox(this);

        QLabel *timerLabel = new QLabel("Work on timer:", this);
        timerWorkCheck = new QCheckBox(this);
        timer = new QTimer(this);

        QLabel *intervalLabel = new QLabel("Interval, ms:", this);
        intervalEdit = new QLineEdit("1000", this);

        QLabel *xorLabel = new QLabel("8-byte value XOR (hex):", this);
        xorValueEdit = new QLineEdit("0000000000000000", this);

        progressBar = new QProgressBar(this);
        progressBar->setRange(0, 100);
        statusLabel = new QLabel("Ready for work", this);

        startButton = new QPushButton("Start", this);
        stopButton = new QPushButton("Stop", this);
        stopButton->setEnabled(false);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(inputPathLabel);
        layout->addWidget(inputPathEdit);
        layout->addWidget(inputPathButton);
        layout->addWidget(maskLabel);
        layout->addWidget(inputFileMask);
        layout->addWidget(selectFileButton);
        layout->addWidget(deleteLabel);
        layout->addWidget(checkDeleteInputFiles);
        layout->addWidget(pathLabel);
        layout->addWidget(resFileSavePath);
        layout->addWidget(browsePathButton);
        layout->addWidget(actionLabel);
        layout->addWidget(overwriteFile);
        layout->addWidget(timerLabel);
        layout->addWidget(timerWorkCheck);
        layout->addWidget(intervalLabel);
        layout->addWidget(intervalEdit);
        layout->addWidget(xorLabel);
        layout->addWidget(xorValueEdit);
        layout->addWidget(progressBar);
        layout->addWidget(statusLabel);
        layout->addWidget(startButton);
        layout->addWidget(stopButton);

        setLayout(layout);
    }

    void connectSignals()
    {
        connect(selectFileButton, &QPushButton::clicked, this, &Window::onSelectFileClicked);
        connect(startButton, &QPushButton::clicked, this, &Window::onStartClicked);
        connect(stopButton, &QPushButton::clicked, this, &Window::onStopClicked);
        connect(browsePathButton, &QPushButton::clicked, this, &Window::onBrowseClicked);
        connect(inputPathButton, &QPushButton::clicked, this, &Window::onInputPathBrowseClicked);
        connect(timer, &QTimer::timeout, this, &Window::onTimeout);

        connect(&fileProcessingWatcher, &QFutureWatcher<void>::finished, this, [this]() {
            if (currentFileIndex < fileList.size() - 1)
            {
                currentFileIndex++;
                if (timerWorkCheck->isChecked() && isProcessing)
                {
                    timer->start(intervalEdit->text().toInt());
                }
                else
                    if (!timerWorkCheck->isChecked() && isProcessing)
                    {
                        onTimeout();
                    }
            }
            else
            {
                onStopClicked();
                statusLabel->setText("Mod end");
                QMessageBox::information(this, "Ready", "All files mod");
            }
        });
    }

    void processFile(const QString& inputPath, const QString& outputPath)
    {
        QElapsedTimer timer;
        timer.start();

        QFile inputFile(inputPath);
        if (!inputFile.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this, inputPath]() {
                statusLabel->setText("Error of open: " + QFileInfo(inputPath).fileName());
            }, Qt::QueuedConnection);
            return;
        }

        if (QFile::exists(outputPath)) {
            if (!overwriteFile->isChecked()) {
                QMetaObject::invokeMethod(this, [this, outputPath]() {
                    statusLabel->setText("Skip, file exists: " + QFileInfo(outputPath).fileName());
                }, Qt::QueuedConnection);
                inputFile.close();
                return;
            }
            QFile::remove(outputPath);
        }

        QFile outputFile(outputPath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            QMetaObject::invokeMethod(this, [this, outputPath]() {
                statusLabel->setText("Error of create: " + QFileInfo(outputPath).fileName());
            }, Qt::QueuedConnection);
            inputFile.close();
            return;
        }

        const qint64 bufferSize = 64 * 1024 * 1024;
        QByteArray xorPattern = QByteArray::fromHex(xorValueEdit->text().toLatin1());
        if (xorPattern.size() < 8) {
            xorPattern = QByteArray(8, '\0');
        }

        qint64 totalBytes = inputFile.size();
        qint64 bytesProcessed = 0;
        qint64 lastUpdateTime = 0;

        while (!inputFile.atEnd() && isProcessing) {
            QByteArray chunk = inputFile.read(bufferSize);
            if (chunk.isEmpty()) break;

            for (int i = 0; i < chunk.size(); ++i) {
                chunk[i] = chunk[i] ^ xorPattern[i % 8];
            }

            if (outputFile.write(chunk) == -1) {
                QMetaObject::invokeMethod(this, [this]() {
                    statusLabel->setText("Error of write");
                }, Qt::QueuedConnection);
                break;
            }

            bytesProcessed += chunk.size();
            qint64 elapsed = timer.elapsed();

            if (elapsed - lastUpdateTime > 200 || bytesProcessed == totalBytes) {
                double progress = (static_cast<double>(bytesProcessed) / totalBytes) * 100;
                double speedMBperSec = (bytesProcessed / (1024.0 * 1024.0)) / (elapsed / 1000.0);

                QMetaObject::invokeMethod(this, [this, progress, speedMBperSec, inputPath]() {
                    progressBar->setValue(static_cast<int>(progress));
                    statusLabel->setText(
                        QString("Process %1: %2% (%3 MB/s)")
                            .arg(QFileInfo(inputPath).fileName())
                            .arg(progress, 0, 'f', 1)
                            .arg(speedMBperSec, 0, 'f', 2));
                }, Qt::QueuedConnection);

                lastUpdateTime = elapsed;
            }
        }

        inputFile.close();
        outputFile.close();

        if (!isProcessing) {
            QFile::remove(outputPath);
        } else if (checkDeleteInputFiles->isChecked()) {
            QFile::remove(inputPath);
        }
    }
};

void Window::onStartClicked()
{
    if (isProcessing) {
        QMessageBox::information(this, "Inf", "Inf is already run");
        return;
    }

    QString inputPath = inputPathEdit->text().trimmed();
    if (inputPath.isEmpty() || !QDir(inputPath).exists()) {
        QMessageBox::warning(this, "Error", "Write/choose correct dir with src");
        return;
    }

    QString mask = inputFileMask->text().trimmed();
    if (mask.isEmpty()) {
        QMessageBox::warning(this, "Error", "Specify mask");
        return;
    }

    QString savePath = resFileSavePath->text().trimmed();
    if (savePath.isEmpty() || !QDir(savePath).exists()) {
        QMessageBox::warning(this, "Error", "Specidy correct dir for save");
        return;
    }

    QDir dir(inputPath);
    fileList = dir.entryInfoList(QStringList(mask), QDir::Files | QDir::NoDotAndDotDot);

    if (fileList.isEmpty()) {
        QMessageBox::information(this, "Inf",
                                 QString("File by mask '%1' not finde in dir:\n%2").arg(mask).arg(inputPath));
        return;
    }

    currentFileIndex = 0;
    isProcessing = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    progressBar->setValue(0);
    statusLabel->setText(QString("Start %1 file").arg(fileList.size()));

    if (timerWorkCheck->isChecked()) {
        timer->start(intervalEdit->text().toInt());
    } else {
        onTimeout();
    }
}

void Window::onStopClicked()
{
    if (!isProcessing) return;

    isProcessing = false;
    timer->stop();
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    statusLabel->setText("Prcs stop by user");
}

void Window::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Choose folder to save", resFileSavePath->text());
    if (!dir.isEmpty()) {
        resFileSavePath->setText(dir);
    }
}

void Window::onInputPathBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Choose folder with src", inputPathEdit->text());
    if (!dir.isEmpty()) {
        inputPathEdit->setText(dir);
    }
}

void Window::onSelectFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Choose file for mod",
        inputPathEdit->text(),
        "All (*.*);;Text (*.txt);;Binary (*.bin)"
        );

    if (!filePath.isEmpty()) {
        fileList.clear();
        fileList.append(QFileInfo(filePath));
        inputPathEdit->setText(QFileInfo(filePath).path());
        inputFileMask->setText(QFileInfo(filePath).fileName());
        statusLabel->setText("Choosen file: " + QFileInfo(filePath).fileName());
    }
}

void Window::onTimeout()
{
    if (!isProcessing || currentFileIndex >= fileList.size()) {
        return;
    }

    QFileInfo fileInfo = fileList[currentFileIndex];
    QString outputPath = QDir(resFileSavePath->text()).filePath(
        fileInfo.completeBaseName() + "_processed." + fileInfo.suffix()
        );

    fileProcessingWatcher.setFuture(QtConcurrent::run([this, fileInfo, outputPath]() {
        processFile(fileInfo.absoluteFilePath(), outputPath);
    }));
}

#include "osn.moc"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Window window;
    window.setWindowTitle("XOR");
    window.resize(600, 500);
    window.show();

    return app.exec();
}
