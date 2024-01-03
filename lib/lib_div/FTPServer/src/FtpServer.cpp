/*
 * FTP Serveur for ESP8266
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva david@nailbuster.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//  2017: modified by @robo8080
// 2019: modified by @fa1ke5

#include "ftpServer.h"

#ifdef ESP32
#define F(A) A
#endif

void FtpServer::begin(String uname, String pword, FS *fp) {

  if (is_up) return;

  // Tells the ftp server to begin listening for incoming connection
	_FTP_USER = uname;
	_FTP_PASS = pword;

  ftpServerp = new WiFiServer(FTP_CTRL_PORT);
  if (!ftpServerp) return;
  dataServer = new WiFiServer(FTP_DATA_PORT_PASV);
  if (!dataServer) return;

  ftpServerp->begin();
	delay(10);
	dataServer->begin();	
	delay(10);
	millisTimeOut = (uint32_t)FTP_TIME_OUT * 60 * 1000;
	millisDelay = 0;
	cmdStatus = 0;
  iniVariables();

  ufsp = fp;

  is_up = true;

#ifdef FTP_DEBUG
	Serial.println(F("Ftp server init complete! "));
#endif
}

void FtpServer::iniVariables() {
  // Default for data port
  dataPort = FTP_DATA_PORT_PASV;
  
  // Default Data connection is Active
  dataPassiveConn = true;
  
  // Set the root directory
  strcpy( cwdName, "/" );

  rnfrCmd = false;
  transferStatus = 0;
  
}

void FtpServer::handleFTP() {

  if (!is_up) return;

  if ((int32_t) ( millisDelay - millis() ) > 0 )
    return;

  if (ftpServerp->hasClient()) {
//  if (ftpServerp->available()) {
	  client.stop();
	  client = ftpServerp->available();
  }
  
  if ( cmdStatus == 0 ) {
    if ( client.connected()) {
      disconnectClient();
    }
    cmdStatus = 1;
  }
  else if ( cmdStatus == 1 )         // Ftp server waiting for connection
  {
    abortTransfer();
    iniVariables();
#ifdef FTP_DEBUG
	  Serial.println(F("Ftp server waiting for connection on port ") + String(FTP_CTRL_PORT));
#endif
    cmdStatus = 2;
  }
  else if ( cmdStatus == 2 )         // Ftp server idle
  {
   		
    if (client.connected() ) {
      // A client connected
      clientConnected();      
      millisEndConnection = millis() + 10 * 1000 ; // wait client id during 10 s.
      cmdStatus = 3;
#ifdef FTP_DEBUG
    	Serial.println(F("Ftp client connected on port ") + String(FTP_CTRL_PORT));
#endif
    }
  }
  else if ( readChar() > 0 )         // got response
  {

    if ( cmdStatus == 3 ) {    
      // Ftp server waiting for user identity
      if (userIdentity()) {
        cmdStatus = 4;
      } else {
        cmdStatus = 0;
      }
    }
    else if ( cmdStatus == 4 ) {
      // Ftp server waiting for user registration
      if (userPassword()) {
        cmdStatus = 5;
        millisEndConnection = millis() + millisTimeOut;
      } else {
        cmdStatus = 0;
      }
    }
    else if ( cmdStatus == 5 ) {
      // Ftp server waiting for user command
      if (!processCommand()) {
        cmdStatus = 0;
      } else {
        millisEndConnection = millis() + millisTimeOut;
      }
    }
  }
  else if (!client.connected() || !client) {
	  cmdStatus = 1;
#ifdef FTP_DEBUG
	    Serial.println(F("client disconnected"));
#endif
  }

  if (transferStatus == 1 ) {
    // Retrieve data
    if (!doRetrieve()) {
      transferStatus = 0;
    }
  } else if ( transferStatus == 2 ) {
    // Store data
    if (!doStore()) {
      transferStatus = 0;
    }
  } else if ( cmdStatus > 2 && ! ((int32_t) ( millisEndConnection - millis() ) > 0 )) {
	  client.println(F("530 Timeout"));
    millisDelay = millis() + 200;    // delay of 200 ms
    cmdStatus = 0;
  }
}

void FtpServer::clientConnected() {
#ifdef FTP_DEBUG
	Serial.println(F("Client connected!"));
#endif
  client.println(F("220--- Welcome to FTP for ESP8266 ---"));
  client.println(F("220---   By David Paiva   ---"));
  client.println(F("220 --   Version ")+ String(FTP_SERVER_VERSION) +"   --");
  iCL = 0;
}

void FtpServer::disconnectClient() {
#ifdef FTP_DEBUG
	Serial.println(F(" Disconnecting client"));
#endif
  abortTransfer();
  client.println(F("221 Goodbye"));
  client.stop();
}

boolean FtpServer::userIdentity() {	
  if (strcmp( command, "USER"))
    client.println(F("500 Syntax error"));
  if (strcmp( parameters, _FTP_USER.c_str() ))
    client.println(F("530 user not found"));
  else {
    client.println(F("331 OK. Password required"));
    strcpy( cwdName, "/" );
    return true;
  }
  millisDelay = millis() + 100;  // delay of 100 ms
  return false;
}

boolean FtpServer::userPassword() {
  if (strcmp( command, "PASS" )) {
    client.println(F("500 Syntax error"));
  } else {
    if (strcmp( parameters, _FTP_PASS.c_str() )) {
      client.println(F("530 "));
    } else {
#ifdef FTP_DEBUG
      Serial.println(F("OK. Waiting for commands."));
#endif
      client.println(F("230 OK."));
      return true;
    }
  }
  millisDelay = millis() + 100;  // delay of 100 ms
  return false;
}

boolean FtpServer::processCommand() {
  ///////////////////////////////////////
  //                                   //
  //      ACCESS CONTROL COMMANDS      //
  //                                   //
  ///////////////////////////////////////


  #ifdef FTP_DEBUG
    //Serial.println(F("curr cmd ") + String(command));
  #endif

  //
  //  CDUP - Change to Parent Directory 
  //
  if( ! strcmp( command, "CDUP" ) || ( ! strcmp( command, "CWD" ) && ! strcmp( parameters, ".." )))
  {
	 bool ok = false;
	 if( strlen( cwdName ) > 1 )            // do nothing if cwdName is root
    {
      // if cwdName ends with '/', remove it (must not append)
      if( cwdName[ strlen( cwdName ) - 1 ] == '/' )
        cwdName[ strlen( cwdName ) - 1 ] = 0;
      // search last '/'
      char * pSep = strrchr( cwdName, '/' );
      ok = pSep > cwdName;
      // if found, ends the string on its position
      if( ok )
      {
        * pSep = 0;
        ok = ufsp->exists( cwdName );
      }
    }
    // if an error appends, move to root
    if( ! ok )
      strcpy( cwdName, "/" );
   // client << F("250 Ok. Current directory is ") << cwdName << eol;
	 
	 client.println(F("250 Ok. Current directory is ") + String(cwdName));
  }
  //
  //  CWD - Change Working Directory
  //
  else if( ! strcmp( command, "CWD" ))
  { 
    
    
  char path[ FTP_CWD_SIZE ];
    if ( haveParameter() && makeExistsPath( path ))
    {
      strcpy( cwdName, path );
       client.println(F("250 Ok. Current directory is ") + String(cwdName) );
    }  
    
    
    //Serial.print("********************************************cwdName: ");Serial.println(String(cwdName));
    /*
    char path[ FTP_CWD_SIZE ];
    if( strcmp( parameters, "." ) == 0 ){  // 'CWD .' is the same as PWD command
    
      client.println( "257 \"" + String(cwdName) + "\" is your current directory");
      */
   //   Serial.print("********************************************cwdName: ");Serial.println(String(cwdName));
   /*
    }
    else 
      {   
        if( haveParameter() && makeExistsPath( path )){  
         strcpy( cwdName, path );
        Serial.print("************************parameters: ");Serial.println(parameters);
        
        client.println( "250 Ok. Current directory is " + String(cwdName) );
         Serial.print("********************************************cwdName: ");Serial.println(String(cwdName));
        }
      }
    */
  }
  //
  //  PWD - Print Directory
  //
  else if( ! strcmp( command, "PWD" ))
    client.println(F("257 \"") + String(cwdName) + "\" is your current directory");
  //
  //  QUIT
  //
  else if( ! strcmp( command, "QUIT" ))
  {
    disconnectClient();
    return false;
  }

  ///////////////////////////////////////
  //                                   //
  //    TRANSFER PARAMETER COMMANDS    //
  //                                   //
  ///////////////////////////////////////

  //
  //  MODE - Transfer Mode 
  //
  else if( ! strcmp( command, "MODE" ))
  {
    if( ! strcmp( parameters, "S" ))
      client.println(F("200 S Ok"));
    // else if( ! strcmp( parameters, "B" ))
    //  client.println( "200 B Ok\r\n";
    else
      client.println(F("504 Only S(tream) is suported"));
  }
  //
  //  PASV - Passive Connection management
  //
  else if( ! strcmp( command, "PASV" ))
  {
    if (data.connected()) data.stop();
    //dataServer->begin();
     //dataIp = Ethernet.localIP();    
	dataIp = WiFi.localIP();	
	dataPort = FTP_DATA_PORT_PASV;
    //data.connect( dataIp, dataPort );
    //data = dataServer->available();
#ifdef FTP_DEBUG
	Serial.println(F("Connection management set to passive"));
  Serial.println(F("Data port set to ") + String(dataPort));
#endif
   client.println(F("227 Entering Passive Mode (") + String(dataIp[0]) + "," + String(dataIp[1])+","+ String(dataIp[2])+","+ String(dataIp[3])+","+String( dataPort >> 8 ) +","+String ( dataPort & 255 )+").");
   dataPassiveConn = true;
  }
  //
  //  PORT - Data Port
  //
  else if( ! strcmp( command, "PORT" ))
  {
	  if (data) data.stop();
    // get IP of data client
    dataIp[ 0 ] = atoi( parameters );
    char * p = strchr( parameters, ',' );
    for( uint8_t i = 1; i < 4; i ++ )
    {
      dataIp[ i ] = atoi( ++ p );
      p = strchr( p, ',' );
    }
    // get port of data client
    dataPort = 256 * atoi( ++ p );
    p = strchr( p, ',' );
    dataPort += atoi( ++ p );
    if( p == NULL )
      client.println(F("501 Can't interpret parameters"));
    else
    {
      client.println(F("200 PORT command successful"));
      dataPassiveConn = false;
    }
  }
  //
  //  STRU - File Structure
  //
  else if( ! strcmp( command, "STRU" ))
  {
    if( ! strcmp( parameters, "F" ))
      client.println(F("200 F Ok"));
    // else if( ! strcmp( parameters, "R" ))
    //  client.println( "200 B Ok\r\n";
    else
      client.println(F("504 Only F(ile) is suported"));
  }
  //
  //  TYPE - Data Type
  //
  else if( ! strcmp( command, "TYPE" ))
  {
    if( ! strcmp( parameters, "A" ))
      client.println(F("200 TYPE is now ASII"));
    else if( ! strcmp( parameters, "I" ))
      client.println(F("200 TYPE is now 8-bit binary"));
    else
      client.println(F("504 Unknow TYPE"));
  }

  ///////////////////////////////////////
  //                                   //
  //        FTP SERVICE COMMANDS       //
  //                                   //
  ///////////////////////////////////////

  //
  //  ABOR - Abort
  //
  else if( ! strcmp( command, "ABOR" ))
  {
    abortTransfer();
    client.println(F("226 Data connection closed"));
  }
  //
  //  DELE - Delete a File 
  //
  else if( ! strcmp( command, "DELE" ))
  {
    char path[ FTP_CWD_SIZE ];
    if (!haveParameter())
      client.println(F("501 No file name"));
    else if( makePath( path ))
    {
      if ( ! ufsp->exists( path ))
        client.println(F("550 File ") + String(parameters) + " not found");
      else
      {
        if ( ufsp->remove( path ))
          client.println(F("250 Deleted ") + String(parameters) );
        else
          client.println(F("450 Can't delete ") + String(parameters));
      }
    }
  }
  //
  //  LIST - List 
  //

  
 
  else if( ! strcmp( command, "LIST" ))
  {

#ifdef FTP_DEBUG
      Serial.println(F("LIST path = ") + String(cwdName) );
#endif

     if (dataConnect()) {
      client.println(F("150 Accepted data connection"));
      uint16_t nm = 0;

      File dir = ufsp->open(cwdName, "r");
      dir.rewindDirectory();
     if ((!dir) || (!dir.isDirectory()))
        client.println(F("550 Can't open directory ") + String(cwdName) );
      else
      {
        File file = dir.openNextFile();
        while( file)
        {
          String fn, fs;
          fn = file.name();
          int i = fn.lastIndexOf("/")+1;
          fn.remove(0, i);
#ifdef FTP_DEBUG
          Serial.println(F("File Name = ") + fn);
#endif
          fs = String(file.size());
          uint32_t tm = file.getLastWrite();
          String tstr = GetDT(tm);
          tstr[10] = ' ';
          if (file.isDirectory()) {
            data.println(tstr + F(" <DIR> ") + fn);
          } else {
            data.println(tstr + " " + fs + " " + fn);
          }
          nm ++;
          file = dir.openNextFile();
        }
        client.println( "226 " + String(nm) + F(" matches total"));
        data.stop();
      }
      
      }
      else{
        client.println(F("425 No data connection"));
        data.stop();
        }    
  }
  
  //
  //  MLSD - Listing for Machine Processing (see RFC 3659)
  //
  else if( ! strcmp( command, "MLSD" ))
  {
    if( ! dataConnect())
      client.println(F("425 No data connection MLSD"));
    else
    {
	  client.println(F("150 Accepted data connection"));
      uint16_t nm = 0;
//      Dir dir= SD.openDir(cwdName);
      File dir= ufsp->open(cwdName, "r");
      char dtStr[ 15 ];
    //  if(!SD.exists(cwdName))
     if((!dir)||(!dir.isDirectory()))
        client.println(F("550 Can't open directory ") +String(cwdName) );
//        client.println( "550 Can't open directory " +String(parameters) );
      else
      {
//        while( dir.next())
        File file = dir.openNextFile();
//        while( dir.openNextFile())
        while( file)
    		{
        
    			String fn,fs;
          fn = file.name();
          int pos = fn.lastIndexOf("/"); //ищем начало файла по последнему "/"
          fn.remove(0, pos+1); //Удаляем все до имени файла включительно
          fs = String(file.size());
          if(file.isDirectory()){
	  
//	      data.println(fn);
              data.println(F("Type=dir;Size=") + fs + ";"+"modify=20000101000000;" +" " + fn);

          } else {
//	      data.println( fs + " " + fn);
              data.println(F("Type=file;Size=") + fs + ";"+"modify=20000101160656;" +" " + fn);
	      
          }
          nm ++;
          file = dir.openNextFile();
        }
        client.println(F("226-options: -a -l"));
        client.println( "226 " + String(nm) + F(" matches total"));
      }
      data.stop();
    }
  }
  //
  //  NLST - Name List 
  //
  else if( ! strcmp( command, "NLST" ))
  {
    if( ! dataConnect())
      client.println(F("425 No data connection"));
    else
    {
      client.println(F("150 Accepted data connection"));
      uint16_t nm = 0;
//      Dir dir=SD.openDir(cwdName);
      File dir= ufsp->open(cwdName, "r");
      if( !ufsp->exists( cwdName ))
        client.println(F("550 Can't open directory ") + String(parameters));
      else
      {
          File file = dir.openNextFile();
//        while( dir.next())
        while( file)
        {
//          data.println( dir.fileName());
          data.println( file.name());
          nm ++;
          file = dir.openNextFile();
        }
        client.println(F("226 ") + String(nm) + F(" matches total"));
      }
      data.stop();
    }
  }
  //
  //  NOOP
  //
  else if( ! strcmp( command, "NOOP" ))
  {
    // dataPort = 0;
    client.println(F("200 Zzz..."));
  }
  //
  //  RETR - Retrieve
  //
  else if( ! strcmp( command, "RETR" )) {
    char path[ FTP_CWD_SIZE ];
#ifdef FTP_DEBUG
  	Serial.println(F("retr ") + String(parameters));
#endif
    if (!haveParameter()) {
      client.println(F("501 No file name"));
    }
    else {
      if ( makePath( path )) {
		    file = ufsp->open(path, "r");
        if( !file){
          client.println(F("550 File ") +String(parameters)+ F(" not found"));
          client.println(F("450 Can't open ") +String(parameters));
        }
        else if( ! dataConnect())
          client.println(F("425 No data connection"));
        else {
#ifdef FTP_DEBUG
  		    Serial.println(F("Sending ") + String(parameters));
#endif
          client.println(F("150-Connected to port ")+ String(dataPort));
          client.println( "150 " + String(file.size()) + F(" bytes to download"));
          millisBeginTrans = millis();
          bytesTransfered = 0;
          transferStatus = 1;
        }
      }
    }
  }
  //
  //  STOR - Store
  //
  else if (!strcmp( command, "STOR" ))
  {
    char path[ FTP_CWD_SIZE ];
    if (!haveParameter()) {
      client.println(F("501 No file name"));
    } else {
      if( makePath( path )) {
		    file = ufsp->open(path, "w");
        if (!file) {
          client.println(F("451 Can't open/create ") +String(parameters) );
        } else {
          if ( ! dataConnect())  {
            client.println(F("425 No data connection"));
            file.close();
          } else {
  #ifdef FTP_DEBUG
            Serial.println(F("Receiving ") + String(parameters));
  #endif
            client.println(F("150 Connected to port ") + String(dataPort));
            millisBeginTrans = millis();
            bytesTransfered = 0;
            transferStatus = 2;
          }
        }
      }
    }
  }
  //
  //  MKD - Make Directory
  //
  
  else if (!strcmp( command, "MKD" ))
  {
     char path[ FTP_CWD_SIZE ];
  #ifdef FTP_DEBUG
            Serial.println(F("mkdir ") + String(parameters));
  #endif
     if (haveParameter() && makePath( path )) {
      if (ufsp->exists( path )) {
        client.println(F("521 Can't create \"") + String(parameters) + F(", Directory exists"));
      } else {
        if ( ufsp->mkdir( path )) {
            client.println( "257 \"" + String(parameters) + F("\" created"));
        } else{
            client.println(F("550 Can't create \"") + String(parameters));
        }
      }  
    }
	 
  }
  //
  //  RMD - Remove a Directory 
  //
  else if( ! strcmp( command, "RMD" ))
  {
	 char path[ FTP_CWD_SIZE ];
     if( haveParameter() && makePath( path )){
      if( ufsp->rmdir( path )){
#ifdef FTP_DEBUG
        Serial.println(F(" Deleting ") +String(parameters));
#endif
        client.println( "250 \"" + String(parameters) + "\" deleted");  
      } else {
        client.println(F("550 Can't remove \"") + String(parameters) + F("\". Directory not empty?"));  
      }
    }
  }
  //
  //  RNFR - Rename From 
  //
  else if ( ! strcmp( command, "RNFR" ))
  {
    buf[ 0 ] = 0;
    if( strlen( parameters ) == 0 ) {
      client.println(F("501 No file name"));
    } else {
      if ( makePath( buf )) {
        if ( ! ufsp->exists( buf )) {
          client.println(F("550 File ") + String(parameters)+ F(" not found"));
        } else {
#ifdef FTP_DEBUG
	        Serial.println(F("Renaming ") + String(buf));
#endif
          client.println(F("350 RNFR accepted - file exists, ready for destination"));     
          rnfrCmd = true;
        }
      }
    }
  }
  //
  //  RNTO - Rename To 
  //
  else if ( ! strcmp( command, "RNTO" ))
  {  
    char path[ FTP_CWD_SIZE ];
    char dir[ FTP_FIL_SIZE ];
    if ( strlen( buf ) == 0 || ! rnfrCmd ) {
      client.println(F("503 Need RNFR before RNTO"));
    } else {
      if (!haveParameter()) {
        client.println(F("501 No file name"));
      } else {
        if( makePath( path )) {
          if (ufsp->exists( path )) {
            client.println( "553 " +String(parameters)+ F(" already exists"));
          } else {          
#ifdef FTP_DEBUG
		        Serial.println(F("Renaming ") + String(buf) + " to " + String(path));
#endif
            if ( ufsp->rename( buf, path )) {
              client.println(F("250 File successfully renamed or moved"));
            } else {
				      client.println( "451 Rename/move failure");
            }
          }
        }                   
      }
    }
    rnfrCmd = false;
  }

  ///////////////////////////////////////
  //                                   //
  //   EXTENSIONS COMMANDS (RFC 3659)  //
  //                                   //
  ///////////////////////////////////////

  //
  //  FEAT - New Features
  //
  else if( ! strcmp( command, "FEAT" ))
  {
    client.println(F("211-Extensions suported:"));
    client.println(F(" MLSD"));
    client.println(F("211 End."));
  }
  //
  //  MDTM - File Modification Time (see RFC 3659)
  //
  else if (!strcmp(command, "MDTM"))
  {
	  client.println(F("550 Unable to retrieve time"));
  }

  //
  //  SIZE - Size of the file
  //
  else if( ! strcmp( command, "SIZE" ))
  {
    char path[ FTP_CWD_SIZE ];
    if ( strlen( parameters ) == 0 ) {
      client.println(F("501 No file name"));
    } else {
      if( makePath( path )) {
		    file = ufsp->open(path, "r");
        if (!file) {
         client.println(F("450 Can't open ") + String(parameters) );
        } else {
          client.println( "213 " + String(file.size()));
          file.close();
        }
      }
    }
  }
  //
  //  SITE - System command
  //
  else if( ! strcmp( command, "SITE" ))
  {
      client.println(F("500 Unknow SITE command ") +String(parameters) );
  }
  //
  //  Unrecognized commands ...
  //
  else {
    client.println(F("500 Unknow command"));
  }
  return true;
}

boolean FtpServer::dataConnect() {
  unsigned long startTime = millis();
  //wait 5 seconds for a data connection
  if (!data.connected())
  {
    while (!dataServer->hasClient() && millis() - startTime < 10000)
//    while (!dataServer->available() && millis() - startTime < 10000)
	  {
		  //delay(100);
		  yield();
	  }
    if (dataServer->hasClient()) {
//    if (dataServer->available()) {
		  data.stop();
		  data = dataServer->available();
#ifdef FTP_DEBUG
		  Serial.println(F("ftpdataserver client...."));
#endif
		
	  }
  }

  return data.connected();

}

boolean FtpServer::doRetrieve() {
  if (data.connected()) {
    int16_t nb = file.readBytes(buf, FTP_BUF_SIZE);
    if (nb > 0) {
      data.write((uint8_t*)buf, nb);
      bytesTransfered += nb;
      return true;
    }
  }
  closeTransfer();
  return false;
}


boolean FtpServer::doStore() {
  if (data.connected()) {
    int16_t nb = data.readBytes((uint8_t*) buf, FTP_BUF_SIZE );
    if (nb > 0) {
      // Serial.println( millis() << " " << nb << endl;
      file.write((uint8_t*) buf, nb );
      bytesTransfered += nb;
    }
    return true;
  }
  closeTransfer();
  return false;
}

void FtpServer::closeTransfer() {
  uint32_t deltaT = (int32_t) ( millis() - millisBeginTrans );
  if ( deltaT > 0 && bytesTransfered > 0 ) {
    client.println(F("226-File successfully transferred"));
    client.println( "226 " + String(deltaT) + " ms, "+ String(bytesTransfered / deltaT) + " kbytes/s");
  } else {
    client.println(F("226 File successfully transferred"));
  }
  file.close();
  data.stop();
}

void FtpServer::abortTransfer() {
  if (transferStatus > 0) {
    file.close();
    data.stop(); 
    client.println(F("426 Transfer aborted"));
#ifdef FTP_DEBUG
    Serial.println(F("Transfer aborted!")) ;
#endif
  }
  transferStatus = 0;
}

// Read a char from client connected to ftp server
//
//  update cmdLine and command buffers, iCL and parameters pointers
//
//  return:
//    -2 if buffer cmdLine is full
//    -1 if line not completed
//     0 if empty line received
//    length of cmdLine (positive) if no empty line received 

int8_t FtpServer::readChar() {
  int8_t rc = -1;

  if (client.available()) {
    char c = client.read();
  #ifdef FTP_DEBUG
    Serial.print( c);
  #endif
    if (c == '\\') {
      c = '/';
    }
    if (c != '\r') {
      if (c != '\n') {
        if (iCL < FTP_CMD_SIZE) {
          cmdLine[ iCL ++ ] = c;
        } else {
          rc = -2; //  Line too long
        }
      } else {
        cmdLine[ iCL ] = 0;
        command[ 0 ] = 0;
        parameters = NULL;
        // empty line?
        if (iCL == 0) {
          rc = 0;
        } else {
          rc = iCL;
          // search for space between command and parameters
          parameters = strchr( cmdLine, ' ' );
          if (parameters != NULL ) {
            if (parameters - cmdLine > 4) {
              rc = -2; // Syntax error
            } else {
              strncpy( command, cmdLine, parameters - cmdLine );
              command[ parameters - cmdLine ] = 0;
              
              while( * ( ++ parameters ) == ' ' )
                ;
            }
          }
          else if (strlen( cmdLine ) > 4)
            rc = -2; // Syntax error.
          else
            strcpy( command, cmdLine );
          iCL = 0;
        }
      }
    }
    if (rc > 0) {
      for (uint8_t i = 0 ; i < strlen( command ); i ++ ) {
        command[ i ] = toupper( command[ i ] );
      }
    }
    if (rc == -2 ) {
      iCL = 0;
      client.println(F("500 Syntax error"));
    }
  }
  return rc;
}

// Make complete path/name from cwdName and parameters
//
// 3 possible cases: parameters can be absolute path, relative path or only the name
//
// parameters:
//   fullName : where to store the path/name
//
// return:
//    true, if done

boolean FtpServer::makePath( char * fullName ) {
  return makePath( fullName, parameters );
}

boolean FtpServer::makePath( char * fullName, char * param ) {
  if ( param == NULL )
    param = parameters;
    
  // Root or empty?
  if (strcmp( param, "/" ) == 0 || strlen( param ) == 0 ) {
    strcpy( fullName, "/" );
    return true;
  }
  // If relative path, concatenate with current dir
  if (param[0] != '/') {
    strcpy( fullName, cwdName );
    if (fullName[ strlen( fullName ) - 1 ] != '/' ) {
      strncat( fullName, "/", FTP_CWD_SIZE );
    }
    strncat( fullName, param, FTP_CWD_SIZE );
  } else {
    strcpy( fullName, param );
  }
  // If ends with '/', remove it
  uint16_t strl = strlen( fullName ) - 1;
  if (fullName[ strl ] == '/' && strl > 1 )
    fullName[ strl ] = 0;
  if (strlen( fullName ) < FTP_CWD_SIZE )
    return true;

  client.println(F("500 Command line too long"));
  return false;
}

// Calculate year, month, day, hour, minute and second
//   from first parameter sent by MDTM command (YYYYMMDDHHMMSS)
//
// parameters:
//   pyear, pmonth, pday, phour, pminute and psecond: pointer of
//     variables where to store data
//
// return:
//    0 if parameter is not YYYYMMDDHHMMSS
//    length of parameter + space

uint8_t FtpServer::getDateTime( uint16_t * pyear, uint8_t * pmonth, uint8_t * pday,
                                uint8_t * phour, uint8_t * pminute, uint8_t * psecond )
{
  char dt[ 15 ];

  // Date/time are expressed as a 14 digits long string
  //   terminated by a space and followed by name of file
  if (strlen( parameters ) < 15 || parameters[ 14 ] != ' ' )
    return 0;
  for ( uint8_t i = 0; i < 14; i++ ) {
    if (! isdigit( parameters[ i ]))
      return 0;
  }

  strncpy( dt, parameters, 14 );
  dt[ 14 ] = 0;
  * psecond = atoi( dt + 12 ); 
  dt[ 12 ] = 0;
  * pminute = atoi( dt + 10 );
  dt[ 10 ] = 0;
  * phour = atoi( dt + 8 );
  dt[ 8 ] = 0;
  * pday = atoi( dt + 6 );
  dt[ 6 ] = 0 ;
  * pmonth = atoi( dt + 4 );
  dt[ 4 ] = 0 ;
  * pyear = atoi( dt );
  return 15;
}

// Create string YYYYMMDDHHMMSS from date and time
//
// parameters:
//    date, time 
//    tstr: where to store the string. Must be at least 15 characters long
//
// return:
//    pointer to tstr

char * FtpServer::makeDateTimeStr( char * tstr, uint16_t date, uint16_t time ) {
  sprintf( tstr, "%04u%02u%02u%02u%02u%02u",
           (( date & 0xFE00 ) >> 9 ) + 1980, ( date & 0x01E0 ) >> 5, date & 0x001F,
           ( time & 0xF800 ) >> 11, ( time & 0x07E0 ) >> 5, ( time & 0x001F ) << 1 );            
  return tstr;
}

bool FtpServer::haveParameter() {
  if ( parameters != NULL && strlen( parameters ) > 0 ) {
    return true;
  }
  client.println(F("501 No file name"));
  return false;  
}
bool FtpServer::makeExistsPath( char * path, char * param ) {
  if (! makePath( path, param ))
    return false;
  if (ufsp->exists( path ))
    return true;
  client.println("550 " + String(path) + F(" not found."));

  return false;
}
