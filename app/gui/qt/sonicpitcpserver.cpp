#include "sonicpitcpserver.h"
#include "mainwindow.h"

// Qt stuff
#include <QtNetwork>
#include <QTcpSocket>

#include "sonicpiserver.h"

// OSC stuff
#include "oscpkt.hh"

SonicPiTCPServer::SonicPiTCPServer(MainWindow *sonicPiWindow, OscHandler *oscHandler) : SonicPiServer(sonicPiWindow, oscHandler)
{
    tcpServer = new QTcpServer(sonicPiWindow);
    buffer.clear();
    blockSize = 0;

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(client()));
}

void SonicPiTCPServer::startServer(){
    int PORT_NUM = 4558;
    if(tcpServer->listen(QHostAddress::LocalHost, PORT_NUM)){
      std::cout << "TCP OSC Server started: " << PORT_NUM;
      handler->server_started = true;
    }
    else{
      tcpServer->close();
      std::cerr << "Server failed to start!";
    }

 }

void SonicPiTCPServer::stopServer(){
    tcpServer->close();
}

void SonicPiTCPServer::logError(QAbstractSocket::SocketError e){
    std::cerr << "Socket error:" << e;
}

void SonicPiTCPServer::client(){
    //In TCP we have no ack signal as we don't block the main loop.
    //Hence assume if we get a connection from a client its booted and ready.
    QMetaObject::invokeMethod(parent, "serverStarted", Qt::QueuedConnection);
    socket = tcpServer->nextPendingConnection();
    connect(socket, SIGNAL(readyRead()), this, SLOT(readMessage()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(logError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
    std::vector<char>().swap(buffer);
}

void SonicPiTCPServer::readMessage()
{
    while(socket->bytesAvailable() > 0){
        if (blockSize == 0) {
            if (socket->bytesAvailable() < (int)sizeof(quint32)){
                return;
            }

            socket->read((char *)&blockSize, sizeof(quint32));
            blockSize = qToBigEndian(blockSize);
        }

        if (socket->bytesAvailable() < blockSize){
            return;
        }

        buffer.resize(blockSize);
        int bytesRead = socket->read(&buffer[0], blockSize);

        if(bytesRead < 0 || (uint32_t)bytesRead != blockSize) {
            std::cerr << "Error: read: " << bytesRead << " Expected:" << blockSize << "\n";
            blockSize = 0;
            return;
        }
        std::vector<char> tmp(buffer);
        tmp.swap(buffer);
        handler->oscMessage(buffer);
        blockSize = 0;
        std::vector<char>().swap(buffer);
    }
}
