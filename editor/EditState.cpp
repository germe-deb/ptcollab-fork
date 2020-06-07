#include "EditState.h"

#include <QDataStream>
int quantize(int v, int q) { return (v / q) * q; }

Interval MouseEditState::clock_int(int q) const {
  int begin = std::min(start_clock, current_clock);
  int end = std::max(start_clock, current_clock);
  return {quantize(begin, q), quantize(end, q) + q};
}
EditState::EditState()
    : mouse_edit_state({MouseEditState::Type::Nothing, 0, 0, 0, 0}),
      scale(),
      m_current_unit(0),
      m_quantize_clock(1),
      m_quantize_pitch(1) {}

QDataStream &operator<<(QDataStream &out, const Interval &a) {
  return (out << a.start << a.end);
}

QDataStream &operator>>(QDataStream &in, Interval &a) {
  return (in >> a.start >> a.end);
}

QDataStream &operator<<(QDataStream &out, const MouseEditState &a) {
  return (out << qint8(a.type) << a.start_clock << a.current_clock
              << a.start_pitch << a.current_pitch);
}

QDataStream &operator>>(QDataStream &in, MouseEditState &a) {
  qint8 &type_int = *(qint8 *)(&a.type);
  return (in >> type_int >> a.start_clock >> a.current_clock >> a.start_pitch >>
          a.current_pitch);
}

QDataStream &operator<<(QDataStream &out, const Scale &a) {
  return (out << a.clockPerPx << a.pitchPerPx << a.noteHeight << a.pitchOffset);
}

QDataStream &operator>>(QDataStream &in, Scale &a) {
  return (in >> a.clockPerPx >> a.pitchPerPx >> a.noteHeight >> a.pitchOffset);
}

QDataStream &operator<<(QDataStream &out, const EditState &a) {
  return (out << a.mouse_edit_state << a.scale << a.m_current_unit
              << a.m_quantize_clock << a.m_quantize_pitch);
}

QDataStream &operator>>(QDataStream &in, EditState &a) {
  return (in >> a.mouse_edit_state >> a.scale >> a.m_current_unit >>
          a.m_quantize_clock >> a.m_quantize_pitch);
}
