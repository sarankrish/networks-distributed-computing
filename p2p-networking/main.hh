#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

signals:
	void msgReadyToSend(QByteArray datagram);

public slots:
	void gotReturnPressed();
	void messageReceived(QString msg);

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();
signals:
	void datagramReceived(QString msg);
public slots:
	void readPendingDatagrams();
	void sendDatagram(QByteArray datagram);


private:
	int myPortMin, myPortMax;
};

#endif // P2PAPP_MAIN_HH
