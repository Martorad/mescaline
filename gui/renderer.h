#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QQuickImageProvider>
#include <QElapsedTimer>

class PreviewProvider : public QQuickImageProvider {
public:
  PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  QImage requestImage(const QString &, QSize *size, const QSize &) override {
    if (size) *size = image.size();
    return image;
  }
  QImage image;
};

class Renderer : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool running READ running NOTIFY changed)
  Q_PROPERTY(bool previewRunning READ previewRunning NOTIFY changed)
  Q_PROPERTY(int progress READ progress NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString details READ details NOTIFY changed)
  Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY changed)
  Q_PROPERTY(QString resultPath READ resultPath NOTIFY changed)
  Q_PROPERTY(bool systemDark READ systemDark NOTIFY changed)

public:
  explicit Renderer(const QString &cli, QObject *parent = nullptr);
  bool running() const;
  bool previewRunning() const;
  int progress() const;
  QString status() const;
  QString details() const;
  QString previewUrl() const;
  QString resultPath() const;
  bool systemDark() const;

  void setPreviewProvider(PreviewProvider *provider);
  Q_INVOKABLE void start(const QStringList &arguments, const QString &output, bool preview,
                         bool stream = false);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void openResult();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void changed();

private:
  void readProgress();
  void readPixels();
  void handleLine(const QByteArray &line);
  void finish(int exitCode, QProcess::ExitStatus exitStatus);

  const QString cli_;
  QTemporaryDir previewDir_;
  QProcess process_;
  PreviewProvider *provider_ = nullptr;
  QByteArray pending_;
  QByteArray pendingPixels_;
  QElapsedTimer lastPreviewUpdate_;
  QString status_ = "Ready";
  QString details_;
  QString previewUrl_;
  QString resultPath_;
  QString output_;
  bool preview_ = false;
  bool stream_ = false;
  bool cancelling_ = false;
  int progress_ = 0;
  int rowBytes_ = 0;
  int rowsCompleted_ = 0;
  unsigned previewRevision_ = 0;
};
