#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "connectioncontroller.hpp"
#include "faulttablemodel.hpp"
#include "nodetablemodel.hpp"
#include "robotstate.hpp"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("BotCockpit"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.1"));
    QGuiApplication::setOrganizationName(QStringLiteral("BotCockpit"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    RobotState robotState;
    NodeTableModel nodeModel;
    FaultTableModel faultModel;
    ConnectionController connection(&robotState, &nodeModel);

    QObject::connect(&robotState, &RobotState::faultsListChanged, &faultModel,
                     [&faultModel, &robotState]() {
                         faultModel.setFaults(robotState.faultsList());
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("robotState"),
                                             &robotState);
    engine.rootContext()->setContextProperty(QStringLiteral("nodeModel"),
                                             &nodeModel);
    engine.rootContext()->setContextProperty(QStringLiteral("faultModel"),
                                             &faultModel);
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
