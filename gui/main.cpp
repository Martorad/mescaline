#include "renderer.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName("Mescaline");

  QString cli = QCoreApplication::applicationDirPath() + "/mescaline";
  if (!QFile::exists(cli)) cli = QCoreApplication::applicationDirPath() + "/../mescaline";
  if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == "--cli") {
    cli = QString::fromLocal8Bit(argv[2]);
  }
  const bool smoke = argc == 4 && QString::fromLocal8Bit(argv[3]) == "--smoke-test";
  if (argc != 1 && !(argc == 3 && QString::fromLocal8Bit(argv[1]) == "--cli") && !smoke) {
    qWarning("Usage: mescaline-gui [--cli /path/to/mescaline]");
    return 2;
  }

  Renderer renderer(cli);
  QQmlApplicationEngine engine;
  auto *previewProvider = new PreviewProvider;
  engine.addImageProvider("preview", previewProvider);
  renderer.setPreviewProvider(previewProvider);
  engine.rootContext()->setContextProperty("renderer", &renderer);
  engine.load(QUrl("qrc:/qt/qml/Mescaline/Main.qml"));
  if (engine.rootObjects().isEmpty()) return 1;
  if (smoke && (engine.rootObjects().first()->property("width").toInt() != 1700 ||
                !engine.rootObjects().first()->findChild<QObject *>("settingsDialog") ||
                !engine.rootObjects().first()->findChild<QObject *>("previewPane"))) {
    qWarning("GUI layout smoke test failed");
    return 1;
  }
  if (smoke) {
    QObject *root = engine.rootObjects().first();
    auto *settings = root->findChild<QObject *>("settingsDialog");
    auto *help = root->findChild<QObject *>("expressionHelp");
    auto *rows = root->findChild<QObject *>("expressionRows");
    auto *categories = root->findChild<QObject *>("settingsCategories");
    auto *appearance = root->findChild<QObject *>("appearancePane");
    auto *performance = root->findChild<QObject *>("performancePane");
    auto *scaleInput = root->findChild<QObject *>("scaleInput");
    auto *scalar = root->findChild<QObject *>("scalarExpression");
    auto *red = root->findChild<QObject *>("redExpression");
    auto *canvasWidth = root->findChild<QObject *>("canvasWidth");
    auto *reduced = root->findChild<QObject *>("reducedPreview");
    auto *scale = root->findChild<QObject *>("previewScale");
    auto *threads = root->findChild<QObject *>("threadsSetting");
    auto *live = root->findChild<QObject *>("livePreview");
    if (!settings || !help || !rows || !categories || !appearance || !performance || !scaleInput || !scalar || !red ||
        !canvasWidth || !reduced || !scale || !threads || !live ||
        threads->property("value").toInt() != 0 || live->property("checked").toBool() ||
        !root->property("previewKey").toString().contains("--width=1000")) {
      qWarning("Preview controls smoke test failed");
      return 1;
    }
    QMetaObject::invokeMethod(settings, "open");
    if (!settings->property("visible").toBool() ||
        settings->property("x").toInt() != (1700 - settings->property("width").toInt()) / 2 ||
        settings->property("y").toInt() != (850 - settings->property("height").toInt()) / 2 ||
        !appearance->property("visible").toBool() || performance->property("visible").toBool()) {
      qWarning("Settings dialog smoke test failed");
      return 1;
    }
    settings->setProperty("category", 1);
    if (appearance->property("visible").toBool() || !performance->property("visible").toBool()) {
      qWarning("Settings categories smoke test failed");
      return 1;
    }
    QMetaObject::invokeMethod(settings, "close");
    QMetaObject::invokeMethod(help, "open");
    if (!help->property("visible").toBool() ||
        help->property("x").toInt() != (1700 - help->property("width").toInt()) / 2 ||
        rows->property("count").toInt() != 32) {
      qWarning("Expression cheat sheet smoke test failed");
      return 1;
    }
    threads->setProperty("value", 4);
    if (!root->property("previewKey").toString().contains("--threads=4")) {
      qWarning("Threads setting smoke test failed");
      return 1;
    }
    scaleInput->setProperty("text", "1,25");
    scalar->setProperty("text", "min(1,2) + 0,5");
    red->setProperty("text", "sin(0,5) + x*0,25");
    if (scaleInput->property("text").toString() != "1.25" ||
        scalar->property("text").toString() != "min(1,2) + 0.5" ||
        red->property("text").toString() != "sin(0.5) + x*0.25") {
      qWarning("Decimal normalization smoke test failed");
      return 1;
    }
    scalar->setProperty("text", "min(0,5; 1,5)");
    if (scalar->property("text").toString() != "min(0.5; 1.5)") {
      qWarning("Function decimal normalization smoke test failed");
      return 1;
    }
    reduced->setProperty("checked", true);
    scale->setProperty("text", "50,5");
    if (scale->property("text").toString() != "50.5" ||
        !root->property("previewKey").toString().contains("--width=505")) {
      qWarning("Preview fraction normalization smoke test failed");
      return 1;
    }
    canvasWidth->setProperty("value", 31);
    if (!root->property("previewKey").toString().contains("--width=16")) {
      qWarning("Preview scale binding smoke test failed");
      return 1;
    }
  }
  if (smoke) QTimer::singleShot(100, &app, [&app, &engine] {
    auto *root = engine.rootObjects().first();
    auto *pane = root->findChild<QObject *>("previewPane");
    auto *live = root->findChild<QObject *>("livePreview");
    const double initialHeight = pane->property("height").toDouble();
    live->setProperty("checked", true);
    QTimer::singleShot(100, &app, [pane, initialHeight, &app] {
      if (initialHeight < 700 || pane->property("height").toDouble() != initialHeight) {
        qWarning("Preview pane changed size after enabling Live");
        app.exit(1);
      } else {
        app.quit();
      }
    });
  });
  return app.exec();
}
