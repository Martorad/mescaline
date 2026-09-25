#include "../renderer.h"

#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class RendererTest : public QObject {
  Q_OBJECT
private slots:
  void previewAndRender();
  void livePreview();
  void streamsRowsBeforeCompletion();
  void reportsErrors();
  void cancellation();
};

static QString cli;

void RendererTest::previewAndRender() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  Renderer renderer(cli);
  renderer.start({"--algorithm=checkerboard", "--width=12", "--height=11", "--threads=2"},
                 QString(), true);
  QTRY_COMPARE(renderer.status(), QString("Preview ready"));
  QVERIFY(QFile::exists(QUrl(renderer.previewUrl()).toLocalFile()));

  const QString output = directory.filePath("image.ppm");
  renderer.start({"--expression=x", "--width=12", "--height=11", "--threads=2"}, output, false);
  QTRY_COMPARE(renderer.status(), QString("Render complete"));
  QCOMPARE(renderer.progress(), 100);
  QCOMPARE(renderer.resultPath(), output);
  QFile image(output);
  QVERIFY(image.open(QIODevice::ReadOnly));
  QVERIFY(image.readAll().startsWith("P6\n12 11\n255\n"));
}

void RendererTest::livePreview() {
  Renderer renderer(cli);
  PreviewProvider provider;
  renderer.setPreviewProvider(&provider);
  renderer.start({"--algorithm=checkerboard", "--width=12", "--height=11", "--threads=4"},
                 QString(), true, true);
  QTRY_COMPARE(renderer.status(), QString("Preview ready"));
  QCOMPARE(provider.image.size(), QSize(12, 11));
  QVERIFY(renderer.previewUrl().startsWith("image://preview/"));
  QCOMPARE(provider.image.pixelColor(0, 0), QColor(255, 255, 255));
  QCOMPARE(provider.image.pixelColor(11, 0), QColor(0, 0, 0));
  QCOMPARE(provider.image.pixelColor(0, 10), QColor(0, 0, 0));

  renderer.start({"--algorithm=checkerboard", "--width=2051", "--height=11", "--threads=4"},
                 QString(), true, true);
  QTRY_COMPARE(renderer.status(), QString("Preview ready"));
  QCOMPARE(provider.image.size(), QSize(684, 4));
}

void RendererTest::streamsRowsBeforeCompletion() {
  QTemporaryDir directory;
  const QString script = directory.filePath("two-rows");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(R"(#!/bin/sh
for arg do case "$arg" in --output=*) output=${arg#--output=};; esac; done
printf 'MPR1\002\000\000\000\002\000\000\000\000\000\000\000\377\000\000\000\377\000'
sleep 2
printf '\001\000\000\000\000\000\377\377\377\377'
printf 'P6\n2 2\n255\n\377\000\000\000\377\000\000\000\377\377\377\377' > "$output"
)");
  file.close();
  QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

  Renderer renderer(script);
  PreviewProvider provider;
  renderer.setPreviewProvider(&provider);
  renderer.start({}, QString(), true, true);
  QTRY_VERIFY_WITH_TIMEOUT(provider.image.size() == QSize(2, 2), 1000);
  QCOMPARE(provider.image.pixelColor(0, 0), QColor(255, 0, 0));
  QCOMPARE(provider.image.pixelColor(0, 1), QColor(0, 0, 0));
  QVERIFY(renderer.running());
  QTRY_COMPARE_WITH_TIMEOUT(renderer.status(), QString("Preview ready"), 5000);
  QCOMPARE(provider.image.pixelColor(0, 1), QColor(0, 0, 255));
}

void RendererTest::reportsErrors() {
  QTemporaryDir directory;
  Renderer renderer(cli);
  renderer.start({"--expression=missing", "--width=1", "--height=1"},
                 directory.filePath("bad.ppm"), false);
  QTRY_COMPARE(renderer.status(), QString("Render failed"));
  QVERIFY(renderer.details().contains("unknown identifier"));
}

void RendererTest::cancellation() {
  QTemporaryDir directory;
  const QString script = directory.filePath("slow-cli");
  QFile file(script);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("#!/bin/sh\nexec sleep 10\n");
  file.close();
  QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

  Renderer renderer(script);
  renderer.start({}, directory.filePath("image.ppm"), false);
  QTRY_VERIFY(renderer.running());
  renderer.cancel();
  QTRY_COMPARE(renderer.status(), QString("Cancelled"));
  QTRY_VERIFY(!renderer.running());
}

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  if (argc != 2) return 2;
  cli = QString::fromLocal8Bit(argv[1]);
  RendererTest test;
  return QTest::qExec(&test, 1, argv);
}

#include "renderer_test.moc"
