
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <string>
#include <time.h>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));

	this->sock = new NetSocket();
	connect(sock, SIGNAL(readyRead()), 
		this, SLOT(readPendingDatagrams()));

	this->timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), 
		this, SLOT(timeoutHandler()));

	this->ae_timer = new QTimer(this);
	connect(ae_timer, SIGNAL(timeout()), 
		this, SLOT(antiEntropy()));
	this->ae_timer->start(10000);


	srand(time(NULL));
	username="local"+QString::number(rand());
	SeqNo = 1;
	QMap<QString, quint32> wants;
	statusMap.insert("Want",wants);
}

void ChatDialog::gotReturnPressed()
{
	qDebug() <<  "Inside gotReturnPressed()" ;
	QByteArray datagram = serialize(textline->text());
	//emit msgReadyToSend(datagram);

	qDebug() << "FIX: send message to other peers: " << textline->text();
	textview->append(username+": "+ textline->text());

	if(statusMap["Want"].contains(username)) {
        statusMap["Want"][username] += 1;
    }
    else {
        statusMap["Want"].insert(username, SeqNo);
	}
	randomPortGenerator();
	sendDatagram(datagram);
	// Clear the textline to get ready for the next input message.
	textline->clear();
}

QByteArray ChatDialog::serialize(QString text){

	QMap<QString, QVariant> msg;
	msg["Origin"]=username;
	msg["ChatText"]=text;
	msg["SeqNo"]= SeqNo;

    if(myMsgs.contains(username)) { 
        myMsgs[username].insert(SeqNo, msg);
    }
    else {
        QMap<quint32, QVariantMap> tmap;
        myMsgs.insert(username, tmap);
        myMsgs[username].insert(SeqNo, msg);
	}
	
	SeqNo++;

	QByteArray datagram;
	QDataStream out(&datagram,QIODevice::ReadWrite);
	
	out << msg;
	return datagram;

}

void ChatDialog::messageReceived(QMap<QString, QVariant> msg){
	qDebug() << "FIX: received message from peer: " << msg;
	if (msg["Origin"]!=username){
		textview->append(msg["Origin"].toString()+": "+msg["ChatText"].toString());
	}
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
	//connect(this,SIGNAL(readyRead()),this,SLOT(readPendingDatagrams()));
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			myPortCurr = p;
			return true;
		}
	}
	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

void ChatDialog::readPendingDatagrams(){
	while (sock->hasPendingDatagrams()){
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(),datagram.size(),&sender,&senderPort);
		remotePort = senderPort;

		QDataStream in(&datagram,QIODevice::ReadOnly);
		QMap<QString, QVariant> msg;
		in >> msg;

		if (msg.contains("Want")) {
			QMap<QString, QMap<QString, quint32> > statusMsg;
			QDataStream status_in(&datagram, QIODevice::ReadOnly);
			status_in >> statusMsg;
			processStatusMsg(statusMsg);
		}
		else if(msg.contains("ChatText")){
			//Rumor Message
			QString origin = msg["Origin"].toString();
			quint32 seqnum = msg["SeqNo"].toUInt();
			if(username != origin) {
				//Rumor message from a peer
				if(statusMap["Want"].contains(origin)) {
					if (statusMap["Want"][origin] == seqnum) {
						statusMap["Want"][origin]=seqnum+1;
						textview->append(origin + ": " + msg["ChatText"].toString());
					}
				}else { 
					qDebug()<< "ACK: "+ origin+msg["SeqNo"].toString();
					statusMap["Want"].insert(origin, seqnum+1);
					textview->append(origin + ": " + msg["ChatText"].toString());
				}
			}

			if(myMsgs.contains(origin)) {
        		myMsgs[origin].insert(seqnum, msg);
    		}
			else {
				QMap<quint32, QMap<QString, QVariant> > qMap;
				myMsgs.insert(origin, qMap);
				myMsgs[origin].insert(seqnum, msg);
			}
			
			QByteArray rumorDatagram;
			QDataStream out_rumor(&rumorDatagram,QIODevice::ReadWrite);
			out_rumor << msg;
			randomPortGenerator();
			sendDatagram(rumorDatagram);
			curr_msg = msg;
			timer->stop();
    		QByteArray status_datagram;
    		QDataStream status_out(&status_datagram,QIODevice::ReadWrite);
    		status_out << statusMap;
			sock->writeDatagram(status_datagram,QHostAddress(QHostAddress::LocalHost),remotePort);
    		timer->start(10000);
		}

		//emit datagramReceived(msg);
	}
}

void ChatDialog::processStatusMsg(QMap<QString, QMap<QString, quint32> > peerStatusMsg)
{
	qDebug() <<  "Inside processStatusMsg()" ;
    QByteArray rumorDatagram;
	QDataStream rumor_out(&rumorDatagram, QIODevice::ReadWrite);
	
	QByteArray status_datagram;
	QDataStream status_out(&status_datagram,QIODevice::ReadWrite);

	for (QMap<QString, quint32>::const_iterator iter = peerStatusMsg["Want"].begin(); iter != peerStatusMsg["Want"].end(); ++iter) {
		qDebug() <<  "Inside loop1" ;
		if(!statusMap["Want"].contains(iter.key())) {
			//self doesnt have peer
			//send status
			statusMap["Want"].insert(iter.key(),1);
			status_out << statusMap;
			randomPortGenerator();
			sendDatagram(status_datagram);
    		timer->start(10000);

        }
	}
	for (QMap<QString, quint32>::const_iterator iter = statusMap["Want"].begin(); iter != statusMap["Want"].end(); ++iter) {
		qDebug() <<  "Inside loop2" ;
		if(!peerStatusMsg["Want"].contains(iter.key())){
			//peer doesnt have self
			//send myMsgs[iter.key()][0];
			qDebug() <<  "Peer doesn't have self" ;
			qDebug() <<  "Iter key"<<iter.key() ;
			qDebug() <<  "my msgs"<<myMsgs[iter.key()];
			rumor_out << myMsgs[iter.key()][0];
			randomPortGenerator();
			sendDatagram(rumorDatagram);
    		timer->start(10000);

        } else if(peerStatusMsg["Want"][iter.key()] < statusMap["Want"][iter.key()]) {
			//self ahead
			//send myMsgs[iter.key()][0];
			qDebug() << "self ahead";
			rumor_out << myMsgs[iter.key()][peerStatusMsg["Want"][iter.key()]];
			randomPortGenerator();
			sendDatagram(rumorDatagram);
    		timer->start(10000);
			
        }
        else if(peerStatusMsg["Want"][iter.key()] > statusMap["Want"][iter.key()]){
			//self behind
			//send status
			qDebug() << "self behind";
			status_out << statusMap;
			randomPortGenerator();
			sendDatagram(status_datagram);
    		timer->start(10000);
		}
		else{
			//same status ?
			continue;

		}
    }

    timer->stop();

} 


void ChatDialog::sendDatagram(QByteArray datagram){
	qDebug() <<  "Inside sendDatagram()" ;
    if (sock->myPortCurr == sock->myPortMin) {
        neighbor = sock->myPortCurr + 1;
    } else if (sock->myPortCurr == sock->myPortMax) {
        neighbor = sock->myPortCurr - 1;
    } else {
        qDebug () << "Flipping a coin ...";
        srand(time(NULL));
	    neighbor = (rand() % 2 == 0) ?  sock->myPortCurr - 1: sock->myPortCurr + 1;
	}

	qDebug() <<  "Neighbor identified as :" << QString::number(neighbor);
	sock->writeDatagram(datagram,QHostAddress(QHostAddress::LocalHost),neighbor);
	timer->start(10000);	

}


void ChatDialog::timeoutHandler() {
    QByteArray datagram;
    QDataStream resend_out(&datagram, QIODevice::ReadWrite);
    resend_out << curr_msg;
	qDebug() <<  "Resending because of timeout :" << QString::number(neighbor);
	sock->writeDatagram(datagram,QHostAddress(QHostAddress::LocalHost),neighbor);
    timer->start(10000);
}

void ChatDialog::randomPortGenerator(){
	if (sock->myPortCurr == sock->myPortMin) {
        neighbor = sock->myPortCurr + 1;
    } else if (sock->myPortCurr == sock->myPortMax) {
        neighbor = sock->myPortCurr - 1;
    } else {
        qDebug () << "Flipping a coin ...";
        srand(time(NULL));
	    neighbor = (rand() % 2 == 0) ?  sock->myPortCurr - 1: sock->myPortCurr + 1;
	}
}

void ChatDialog::antiEntropy(){
	qDebug() <<  "Inside antiEntropy()"; 
    QByteArray datagram;
	QDataStream out(&datagram,QIODevice::ReadWrite);
	qDebug() <<  "Status Map :"<<statusMap;
	randomPortGenerator();
	QString dest="local@"+QString::number(neighbor);
	if(!statusMap["Want"].contains(dest) ){
		statusMap["Want"].insert(dest,1);
	}
	qDebug() <<  "All my messages :"<<myMsgs; 
    out << statusMap;
	sendDatagram(datagram);
    //ae_timer->start(10000);
}


int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();
	qDebug() <<  "Inside main()" ;
	// Create a UDP network socket
	//NetSocket sock;
	if (!dialog.sock->bind())
		exit(1);
	QObject::connect(dialog.sock,SIGNAL(datagramReceived(QMap<QString,QVariant>)),&dialog,SLOT(messageReceived(QMap<QString, QVariant>)));
	//QObject::connect(&dialog,SIGNAL(msgReadyToSend(QByteArray)),dialog.sock,SLOT(sendDatagram(QByteArray)));
	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

