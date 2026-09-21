#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "connectioncontroller.hpp"
#include "nodetablemodel.hpp"
#include "robotstate.hpp"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("BotCockpit"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setOrganizationName(QStringLiteral("BotCockpit"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    RobotState robotState;
    NodeTableModel nodeModel;
    ConnectionController connection(&robotState, &nodeModel);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("robotState"),
                                             &robotState);
    engine.rootContext()->setContextProperty(QStringLiteral("nodeModel"),
                                             &nodeModel);
    engine.rootContext()->setContextProperty(QStringLiteral("connection"),
                                             &connection);

    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
