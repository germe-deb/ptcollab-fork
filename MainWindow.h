#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>
#include <QScrollArea>
#include <QSplitter>
#include <QtMultimedia/QAudioOutput>

#include "EditorScrollArea.h"
#include "KeyboardEditor.h"
#include "PxtoneIODevice.h"
#include "SideMenu.h"
#include "pxtone/pxtnService.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

 private slots:
  void selectAndLoadFile();

 private:
  void loadFile(QString filename);
  void keyPressEvent(QKeyEvent* event);
  QAudioOutput* m_audio;
  KeyboardEditor* m_keyboard_editor;
  pxtnService m_pxtn;
  EditorScrollArea* m_scroll_area;
  PxtoneIODevice m_pxtn_device;
  QSplitter* m_splitter;
  SideMenu* m_side_menu;

  Ui::MainWindow* ui;
};
#endif  // MAINWINDOW_H
