/********************************************************************************
 *   Copyright (C) 2010-2014 by NetResults S.r.l. ( http://www.netresults.it )  *
 *   Author(s):																	*
 *				Francesco Lamonica		<f.lamonica@netresults.it>				*
 ********************************************************************************/

#include "RemoteWriter.h"

#include <QTime>

RemoteWriter::RemoteWriter(const QString &aServerAddress, int aServerPort)
: LogWriter()
{
    m_serverAddress = aServerAddress;
    m_serverPort = aServerPort;

    m_reconnectionTimeout = 5000;

    m_Socket = new QTcpSocket(this);
    m_reconnectionTimer = new QTimer(this);
//    connect (m_reconnectionTimer, SIGNAL(timeout()), this, SLOT(connectToServer()));
    //connect (&m_Socket, SIGNAL(disconnected()), this, SLOT(onDisconnectionFromServer()));
    //connect (&m_Socket, SIGNAL(connected()), this, SLOT(onConnectionToServer()));


    /*
    qDebug() << "this" << this << "this parent" << this->parent() << "this thread" << this->thread();
    //qDebug() << m_logTimer << "logtimer thread" << m_logTimer->thread() << "current thread" << QThread::currentThread();
    qDebug() << m_reconnectionTimer << "reconntimer thread" << m_reconnectionTimer->thread() << "current thread" << QThread::currentThread();
    qDebug() << m_Socket << "qsock thread" << m_Socket->thread() << "current thread" << QThread::currentThread();
    qDebug() << "\n-------------------------------------------------------------\n";
    */
}
 
/*!
  \brief In the class dtor we want to flush whatever we might have got that is not written
  */
RemoteWriter::~RemoteWriter()
{
    //on exit, write all we've got
    this->flush();
}

/*!
  \brief writes the messages in the queue on the socket
  */
void
RemoteWriter::writeToDevice()
{
    ULDBG << Q_FUNC_INFO << "executed in thread" << QThread::currentThread();

    QString s;
    mutex.lock();
    if (!m_logIsPaused && m_Socket->state() == QAbstractSocket::ConnectedState)
    {
        int msgcount = m_logMessageList.count();
        for (int i=0; i<msgcount; i++) {
            s = m_logMessageList.takeFirst().message();
            m_Socket->write(s.toLatin1()+"\r\n");
        }
    }
    mutex.unlock();
}
 
/*!
  \brief connects the logwriter to the specified server
  \return a negative code if the connection fails, 0 otherwise
  This call is blocking (for this thread) and waits 10 secs to see if the connection can be established or not
  */
int
RemoteWriter::connectToServer()
{
    ULDBG << Q_FUNC_INFO << QDateTime::currentDateTime().toString("hh.mm.ss.zzz") << "executed in thread" << QThread::currentThread();
    m_Socket->connectToHost(m_serverAddress, m_serverPort);
    bool b = m_Socket->waitForConnected(10000);
    if (b)
        return 0;

    LogMessage msg("Remote Logger", UNQL::LOG_WARNING, "Connection fail to server "+m_serverAddress+":"+QString::number(m_serverPort),
                   QDateTime::currentDateTime().toString("hh:mm:ss"));
    appendMessage(msg);

    //now we restart the connection timer
    m_reconnectionTimer->start(m_reconnectionTimeout);

    return -1;
}

/*!
 \brief this slot is invoked whenever the RemoteWriter connects to the logging server
 Upon connection it stops the reconnection timer
 */
void
RemoteWriter::onConnectionToServer()
{
    ULDBG << Q_FUNC_INFO << "executed in thread" << QThread::currentThread();
#ifdef ULOGDBG
    qDebug() << "Connected to server";
#endif
    LogMessage msg("Remote Logger", UNQL::LOG_INFO, "Connected to server "+m_serverAddress+":"+QString::number(m_serverPort),
                   QDateTime::currentDateTime().toString("hh:mm:ss"));
    appendMessage(msg);
    m_reconnectionTimer->stop();
    //m_logTimer->start();
}

/*!
 \brief this slot is invoked whenever the RemoteWriter loses connection to the logging server
 It starts a timer that will keep trying to reconnect to the server
 */
void
RemoteWriter::onDisconnectionFromServer()
{
    ULDBG << Q_FUNC_INFO << "executed in thread" << QThread::currentThread();
#ifdef ULOGDBG
    qDebug() << "Disconnected from server";
#endif
    LogMessage msg("Remote Logger", UNQL::LOG_WARNING, "Disconnected from server "+m_serverAddress+":"+QString::number(m_serverPort),
                   QDateTime::currentDateTime().toString("hh:mm:ss"));
    appendMessage(msg);
    m_reconnectionTimer->start(m_reconnectionTimeout);
}
 

//FIXME - protect from multiple calls
void
RemoteWriter::run()
{
    ULDBG << Q_FUNC_INFO;
    LogWriter::run();
//    m_reconnectionTimer = new QTimer(this->thread());
//    m_Socket = new QTcpSocket(this->thread());
    //m_reconnectionTimer = new QTimer();
    //m_Socket = new QTcpSocket();
/*
    qDebug() << "this" << this << "this parent" << this->parent() << "this thread" << this->thread();
    qDebug() << m_logTimer << "logtimer thread" << m_logTimer->thread() << "current thread" << QThread::currentThread();
    qDebug() << m_reconnectionTimer << "reconntimer thread" << m_reconnectionTimer->thread() << "current thread" << QThread::currentThread();
    qDebug() << m_Socket << "qsock thread" << m_Socket->thread() << "current thread" << QThread::currentThread();
*/
    connect (m_reconnectionTimer, SIGNAL(timeout()), this, SLOT(connectToServer()));

    connect (m_Socket, SIGNAL(disconnected()), this, SLOT(onDisconnectionFromServer()));
    connect (m_Socket, SIGNAL(connected()), this, SLOT(onConnectionToServer()));

    //m_reconnectionTimer->start(m_reconnectionTimeout);
    QMetaObject::invokeMethod(this, "connectToServer");
    //LogWriter::run();
}

void
RemoteWriter::setWriterConfig(const WriterConfig &wconf)
{
    LogWriter::setWriterConfig(wconf);
    m_reconnectionTimeout = wconf.reconnectionSecs * 1000;
}
