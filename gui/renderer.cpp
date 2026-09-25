#include "renderer.h"

#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QTimer>
#include <QUrl>

#include <cstring>

static quint32 unpack(const char *bytes) {
  return quint32(uchar(bytes[0])) | (quint32(uchar(bytes[1])) << 8) |
         (quint32(uchar(bytes[2])) << 16) | (quint32(uchar(bytes[3])) << 24);
}

Renderer::Renderer(const QString &cli, QObject *parent) : QObject(parent), cli_(cli) {
  process_.setProcessChannelMode(QProcess::SeparateChannels);
  connect(&process_, &QProcess::readyReadStandardError, this, &Renderer::readProgress);
  connect(&process_, &QProcess::readyReadStandardOutput, this, &Renderer::readPixels);
  connect(&process_, &QProcess::finished, this, &Renderer::finish);
  connect(&process_, &QProcess::stateChanged, this, &Renderer::changed);
  connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      status_ = "Cannot start render";
      details_ = process_.errorString();
      emit changed();
    }
  });
  qGuiApp->installEventFilter(this);
}

bool Renderer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == qGuiApp && event->type() == QEvent::ApplicationPaletteChange) emit changed();
  return QObject::eventFilter(watched, event);
}

bool Renderer::running() const { return process_.state() != QProcess::NotRunning; }
bool Renderer::previewRunning() const { return running() && preview_; }
int Renderer::progress() const { return progress_; }
QString Renderer::status() const { return status_; }
QString Renderer::details() const { return details_; }
QString Renderer::previewUrl() const { return previewUrl_; }
QString Renderer::resultPath() const { return resultPath_; }
bool Renderer::systemDark() const {
  return qGuiApp->palette().color(QPalette::Window).lightness() < 128;
}

void Renderer::setPreviewProvider(PreviewProvider *provider) { provider_ = provider; }

void Renderer::start(const QStringList &arguments, const QString &output, bool preview, bool stream) {
  if (running()) return;
  if (!QFile::exists(cli_) || (preview && !previewDir_.isValid()) || (!preview && output.isEmpty())) {
    status_ = "Cannot start render";
    details_ = !QFile::exists(cli_) ? "CLI not found: " + cli_ : "Choose an output path.";
    emit changed();
    return;
  }

  preview_ = preview;
  stream_ = preview && stream;
  cancelling_ = false;
  progress_ = 0;
  details_.clear();
  status_ = preview ? "Rendering preview…" : "Rendering…";
  pending_.clear();
  pendingPixels_.clear();
  rowBytes_ = 0;
  rowsCompleted_ = 0;
  output_ = preview ? previewDir_.filePath("preview.ppm") : output;
  if (preview) previewUrl_.clear();
  if (preview) QFile::remove(output_); // Only our own temporary file.
  QStringList args = arguments;
  args << "--output=" + output_ << "--progress=json";
  if (stream_) args << "--preview-stream";
  process_.start(cli_, args);
  emit changed();
}

void Renderer::cancel() {
  if (!running() || cancelling_) return;
  cancelling_ = true;
  status_ = "Cancelling…";
  const qint64 pid = process_.processId();
  process_.terminate();
  QTimer::singleShot(3000, this, [this, pid] {
    if (running() && cancelling_ && process_.processId() == pid) process_.kill();
  });
  emit changed();
}

void Renderer::readPixels() {
  if (!stream_ || provider_ == nullptr) {
    process_.readAllStandardOutput();
    return;
  }
  pendingPixels_ += process_.readAllStandardOutput();
  if (rowBytes_ == 0) {
    if (pendingPixels_.size() < 12) return;
    const quint32 width = unpack(pendingPixels_.constData() + 4);
    const quint32 height = unpack(pendingPixels_.constData() + 8);
    if (pendingPixels_.left(4) != "MPR1" || width == 0 || height == 0 ||
        width > 100000 || height > 100000 || quint64(width) * height > 100000000) {
      details_ = "Invalid preview stream header";
      cancel();
      return;
    }
    provider_->image = QImage(int(width), int(height), QImage::Format_RGB888);
    if (provider_->image.isNull()) {
      details_ = "Could not allocate preview image";
      cancel();
      return;
    }
    provider_->image.fill(Qt::black);
    rowBytes_ = int(width) * 3;
    pendingPixels_.remove(0, 12);
    lastPreviewUpdate_.start();
    previewUrl_ = "image://preview/frame?v=" + QString::number(++previewRevision_);
    emit changed();
  }
  while (pendingPixels_.size() >= rowBytes_ + 4) {
    const quint32 y = unpack(pendingPixels_.constData());
    if (y >= quint32(provider_->image.height())) {
      details_ = "Invalid preview row index";
      cancel();
      return;
    }
    std::memcpy(provider_->image.scanLine(int(y)), pendingPixels_.constData() + 4,
                size_t(rowBytes_));
    pendingPixels_.remove(0, rowBytes_ + 4);
    rowsCompleted_++;
    progress_ = 100 * rowsCompleted_ / provider_->image.height();
    status_ = QString("Preview rows %1/%2").arg(rowsCompleted_).arg(provider_->image.height());
    if (lastPreviewUpdate_.elapsed() >= 100) {
      // ponytail: full-image snapshots; use tiled textures if very large previews lag.
      previewUrl_ = "image://preview/frame?v=" + QString::number(++previewRevision_);
      lastPreviewUpdate_.restart();
      emit changed();
    }
  }
}

void Renderer::openResult() {
  if (!resultPath_.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(resultPath_));
}

void Renderer::readProgress() {
  pending_ += process_.readAllStandardError();
  qsizetype newline;
  while ((newline = pending_.indexOf('\n')) >= 0) {
    handleLine(pending_.left(newline));
    pending_.remove(0, newline + 1);
  }
}

void Renderer::handleLine(const QByteArray &line) {
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(line, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    details_ += QString::fromUtf8(line) + '\n';
    emit changed();
    return;
  }
  const QJsonObject event = document.object();
  if (event.value("version").toInt() != 1) {
    details_ += "Unsupported CLI progress version\n";
    emit changed();
    return;
  }
  const QString name = event.value("event").toString();
  if (name == "frame") {
    const int total = event.value("total").toInt();
    if (total > 0) progress_ = 100 * event.value("completed").toInt() / total;
    status_ = QString("Frame %1 of %2").arg(event.value("completed").toInt()).arg(total);
  } else if (name == "encoding") {
    status_ = "Encoding GIF…";
  } else if (name == "warning") {
    details_ += QString("Warning: %1 non-finite expression results\n")
                    .arg(event.value("count").toVariant().toString());
  } else if (name == "error") {
    details_ += event.value("message").toString();
    if (event.contains("system")) details_ += ": " + event.value("system").toString();
    details_ += '\n';
  } else if (name == "cancelled") {
    status_ = "Cancelled";
  } else if (name == "complete") {
    if (!preview_) output_ = event.value("output").toString();
  }
  emit changed();
}

void Renderer::finish(int exitCode, QProcess::ExitStatus exitStatus) {
  readPixels();
  readProgress();
  if (!pending_.isEmpty()) {
    handleLine(pending_);
    pending_.clear();
  }
  if (cancelling_) {
    status_ = "Cancelled";
  } else if (exitStatus == QProcess::NormalExit && exitCode == 0) {
    progress_ = 100;
    status_ = preview_ ? "Preview ready" : "Render complete";
    if (preview_) {
      previewUrl_ = stream_ ? "image://preview/frame?v=" + QString::number(++previewRevision_)
                            : QUrl::fromLocalFile(output_).toString() +
                                  "?v=" + QString::number(++previewRevision_);
    } else {
      resultPath_ = QFile::exists(output_) ? output_ : QString();
    }
  } else {
    status_ = "Render failed";
    if (details_.isEmpty()) details_ = process_.errorString() + '\n';
    details_ += QString("Exit status: %1\n").arg(exitCode);
  }
  cancelling_ = false;
  emit changed();
}
