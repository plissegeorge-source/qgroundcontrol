#include "VehicleTunnelGPS.h"

#include <QtCore/QStringList>
#include <QtCore/QtMath>

#include <cmath>

const QRegularExpression VehicleTunnelGPS::_gpsSegmentRegex(
    QStringLiteral(R"((G[12])\s+([A-Z.]+)\s*\[([^\]]+)\]\s*([NS]\d+\.\d+)\s+([EW]\d+\.\d+)\s+A([+-]?\d+)\s+SP(\d+\.\d+)\s+CS(\d+\.\d+)\s+HD(\d+\.\d+)\s+SV(\d+)\s+SNR(\d+))"),
    QRegularExpression::CaseInsensitiveOption);

const QRegularExpression VehicleTunnelGPS::_useRegex(QStringLiteral(R"(USE:(\d+))"), QRegularExpression::CaseInsensitiveOption);

VehicleTunnelGPS::VehicleTunnelGPS(QObject *parent)
    : QObject(parent)
{
}

bool VehicleTunnelGPS::available() const
{
    return _isFresh(_gps1) || _isFresh(_gps2);
}

QString VehicleTunnelGPS::statusText() const
{
    if (available()) {
        return tr("Live");
    }

    return tr("No tunnel GPS data");
}

QGeoCoordinate VehicleTunnelGPS::gps1Coordinate() const
{
    return _gps1.valid ? QGeoCoordinate(_gps1.lat, _gps1.lon) : QGeoCoordinate();
}

QGeoCoordinate VehicleTunnelGPS::gps2Coordinate() const
{
    return _gps2.valid ? QGeoCoordinate(_gps2.lat, _gps2.lon) : QGeoCoordinate();
}

double VehicleTunnelGPS::gps1Heading() const
{
    return _gps1.headingDeg;
}

double VehicleTunnelGPS::gps2Heading() const
{
    return _gps2.headingDeg;
}

QString VehicleTunnelGPS::gps1Summary() const
{
    if (!_gps1.valid) {
        return tr("GPS 1: no data");
    }

    return tr("GPS 1 · %1 sats · HDOP %2 · %3 m/s")
        .arg(_gps1.satellites)
        .arg(QString::number(_gps1.hdop, 'f', 1))
        .arg(QString::number(_gps1.speed, 'f', 1));
}

QString VehicleTunnelGPS::gps2Summary() const
{
    if (!_gps2.valid) {
        return tr("GPS 2: no data");
    }

    return tr("GPS 2 · %1 sats · HDOP %2 · %3 m/s")
        .arg(_gps2.satellites)
        .arg(QString::number(_gps2.hdop, 'f', 1))
        .arg(QString::number(_gps2.speed, 'f', 1));
}

double VehicleTunnelGPS::distanceMeters() const
{
    if (!_gps1.valid || !_gps2.valid) {
        return qQNaN();
    }

    return _distanceMeters(_gps1.lat, _gps1.lon, _gps2.lat, _gps2.lon);
}

void VehicleTunnelGPS::handleTunnelMessage(const mavlink_message_t &message)
{
    mavlink_tunnel_t tunnel{};
    mavlink_msg_tunnel_decode(&message, &tunnel);

    const int payloadLength = static_cast<int>(tunnel.payload_length);
    if (payloadLength < 4) {
        return;
    }

    const QByteArray rawPayload(reinterpret_cast<const char *>(tunnel.payload), payloadLength);
    const int seq = static_cast<int>(static_cast<uint8_t>(rawPayload[0]));
    const int index = static_cast<int>(static_cast<uint8_t>(rawPayload[1]));
    const int total = static_cast<int>(static_cast<uint8_t>(rawPayload[2]));

    if ((total <= 0) || (total > _maxTunnelChunks) || (index < 0) || (index >= total)) {
        return;
    }

    ChunkState &entry = _chunkMap[seq];
    if ((entry.total != 0) && (entry.total != total)) {
        _chunkMap.remove(seq);
        entry = _chunkMap[seq];
    }

    entry.total = total;
    entry.parts[index] = rawPayload.mid(3);
    entry.updateElapsed.restart();

    if (entry.parts.size() != total) {
        _cleanupStaleChunks();
        return;
    }

    QByteArray assembled;
    assembled.reserve(total * 64);
    for (int i = 0; i < total; ++i) {
        if (!entry.parts.contains(i)) {
            return;
        }
        assembled.append(entry.parts.value(i));
    }

    _chunkMap.remove(seq);
    _processLine(QString::fromLatin1(assembled));
    emit dataChanged();
}

void VehicleTunnelGPS::_cleanupStaleChunks()
{
    for (auto it = _chunkMap.begin(); it != _chunkMap.end();) {
        if (it.value().updateElapsed.isValid() && (it.value().updateElapsed.elapsed() > _chunkStaleMs)) {
            it = _chunkMap.erase(it);
        } else {
            ++it;
        }
    }
}

void VehicleTunnelGPS::_processLine(const QString &line)
{
    const QStringList segments = line.split('|', Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        if (segment.startsWith(QStringLiteral("G1"), Qt::CaseInsensitive)) {
            _updateGpsFromSegment(segment, _gps1);
        } else if (segment.startsWith(QStringLiteral("G2"), Qt::CaseInsensitive)) {
            _updateGpsFromSegment(segment, _gps2);
        }

        const QRegularExpressionMatch useMatch = _useRegex.match(segment);
        if (useMatch.hasMatch()) {
            _selectedSource = useMatch.captured(1).toInt();
        }
    }
}

void VehicleTunnelGPS::_updateGpsFromSegment(const QString &segment, GpsState &gps)
{
    const QRegularExpressionMatch match = _gpsSegmentRegex.match(segment);
    if (!match.hasMatch()) {
        return;
    }

    const auto parseCoordinate = [](const QString &token) {
        if (token.size() < 3) {
            return qQNaN();
        }

        const QChar hemi = token.front().toUpper();
        bool ok = false;
        double value = token.mid(1).toDouble(&ok);
        if (!ok) {
            return qQNaN();
        }

        if ((hemi == QLatin1Char('S')) || (hemi == QLatin1Char('W'))) {
            value *= -1.0;
        }

        return value;
    };

    const double lat = parseCoordinate(match.captured(4));
    const double lon = parseCoordinate(match.captured(5));
    if (!std::isfinite(lat) || !std::isfinite(lon) || (lat == 0.0 && lon == 0.0)) {
        return;
    }

    if (gps.valid) {
        const double dLat = qDegreesToRadians(lat - gps.lat);
        const double dLon = qDegreesToRadians(lon - gps.lon);
        const double y = std::sin(dLon) * std::cos(qDegreesToRadians(lat));
        const double x = std::cos(qDegreesToRadians(gps.lat)) * std::sin(qDegreesToRadians(lat))
            - std::sin(qDegreesToRadians(gps.lat)) * std::cos(qDegreesToRadians(lat)) * std::cos(dLon);
        double heading = qRadiansToDegrees(std::atan2(y, x));
        if (heading < 0.0) {
            heading += 360.0;
        }
        gps.headingDeg = heading;
    }

    gps.valid = true;
    gps.lat = lat;
    gps.lon = lon;
    gps.protocol = match.captured(2);
    gps.filter = match.captured(3).trimmed().toUpper();
    gps.altMeters = match.captured(6).toInt();
    gps.speed = match.captured(7).toDouble();
    gps.hdop = match.captured(9).toDouble();
    gps.satellites = match.captured(10).toInt();
    gps.snr = match.captured(11).toInt();
    gps.updateElapsed.restart();
}

double VehicleTunnelGPS::_distanceMeters(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double earthRadiusM = 6378137.0;
    const double dLat = qDegreesToRadians(lat2 - lat1);
    const double dLon = qDegreesToRadians(lon2 - lon1);

    const double sinLat = std::sin(dLat / 2.0);
    const double sinLon = std::sin(dLon / 2.0);
    const double a = sinLat * sinLat
        + std::cos(qDegreesToRadians(lat1)) * std::cos(qDegreesToRadians(lat2)) * sinLon * sinLon;
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return earthRadiusM * c;
}

bool VehicleTunnelGPS::_isFresh(const GpsState &gps)
{
    return gps.valid && gps.updateElapsed.isValid() && (gps.updateElapsed.elapsed() <= _dataStaleMs);
}
