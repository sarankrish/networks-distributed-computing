#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();
	int myPortMin, myPortMax, myPortCurr;
signals:
	void datagramReceived(QMap<QString,QVariant> msg);

};

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
	NetSocket *sock;

signals:
	void msgReadyToSend(QByteArray datagram);

public slots:
	void gotReturnPressed();
	void messageReceived(QMap<QString, QVariant> msg);
	void timeoutHandler();
	void readPendingDatagrams();
	void antiEntropy();

private:
	QTextEdit *textview;
	QLineEdit *textline;
	QTimer *timer,*ae_timer;
	QString username;
	qint32 SeqNo;
	int neighbor;
	QMap<QString, QMap<quint32, QVariantMap> > myMsgs;
	QMap<QString, QMap<QString, quint32> > statusMap;
	quint16 remotePort;
	QMap<QString, QVariant>  curr_msg;
	void sendDatagram(QByteArray datagram);
	QByteArray serialize(QString text);
	void processStatusMsg(QMap<QString, QMap<QString, quint32> > statusMsg);
	void randomPortGenerator();


};

#endif // P2PAPP_MAIN_HH
