#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QRegularExpression>
#include <QtPositioning/QGeoCoordinate>

#include "QGCMAVLink.h"

class VehicleTunnelGPS : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY dataChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY dataChanged)
    Q_PROPERTY(int selectedSource READ selectedSource NOTIFY dataChanged)
    Q_PROPERTY(QGeoCoordinate gps1Coordinate READ gps1Coordinate NOTIFY dataChanged)
    Q_PROPERTY(QGeoCoordinate gps2Coordinate READ gps2Coordinate NOTIFY dataChanged)
    Q_PROPERTY(double gps1Heading READ gps1Heading NOTIFY dataChanged)
    Q_PROPERTY(double gps2Heading READ gps2Heading NOTIFY dataChanged)
    Q_PROPERTY(QString gps1Summary READ gps1Summary NOTIFY dataChanged)
    Q_PROPERTY(QString gps2Summary READ gps2Summary NOTIFY dataChanged)
    Q_PROPERTY(double distanceMeters READ distanceMeters NOTIFY dataChanged)

public:
    explicit VehicleTunnelGPS(QObject *parent = nullptr);

    bool available() const;
    QString statusText() const;
    int selectedSource() const { return _selectedSource; }
    QGeoCoordinate gps1Coordinate() const;
    QGeoCoordinate gps2Coordinate() const;
    double gps1Heading() const;
    double gps2Heading() const;
    QString gps1Summary() const;
    QString gps2Summary() const;
    double distanceMeters() const;

    void handleTunnelMessage(const mavlink_message_t &message);

signals:
    void dataChanged();

private:
    struct GpsState {
        bool valid = false;
        double lat = qQNaN();
        double lon = qQNaN();
        int altMeters = 0;
        double speed = 0.0;
        double hdop = 0.0;
        int satellites = 0;
        int snr = 0;
        QString protocol;
        QString filter;
        double headingDeg = qQNaN();
        QElapsedTimer updateElapsed;
    };

    struct ChunkState {
        int total = 0;
        QHash<int, QByteArray> parts;
        QElapsedTimer updateElapsed;
    };

    void _cleanupStaleChunks();
    void _processLine(const QString &line);
    void _updateGpsFromSegment(const QString &segment, GpsState &gps);
    static double _distanceMeters(double lat1, double lon1, double lat2, double lon2);
    static bool _isFresh(const GpsState &gps);

    static constexpr int _maxTunnelChunks = 64;
    static constexpr int _chunkStaleMs = 2500;
    static constexpr int _dataStaleMs = 3000;

    static const QRegularExpression _gpsSegmentRegex;
    static const QRegularExpression _useRegex;

    QHash<int, ChunkState> _chunkMap;
    GpsState _gps1;
    GpsState _gps2;
    int _selectedSource = 0;
};
