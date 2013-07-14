//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// LibSourcey is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//


#ifndef SOURCEY_Net_SSLContext_H
#define SOURCEY_Net_SSLContext_H


#include "Sourcey/Memory.h"
#include "Sourcey/Handle.h"

#include "Poco/Crypto/X509Certificate.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/String.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstdlib>


namespace scy {
namespace net {

 
class SSLContext: public CountedObject
	/// This class encapsulates context information for
	/// an SSL server or client, such as the certificate
	/// verification mode and the location of certificates
	/// and private key files, as well as the list of
	/// supported ciphers.
	///
	/// The Context class is also used to control
	/// SSL session caching on the server and client side.
{
public:
	typedef Handle<SSLContext> Ptr;
	
	enum Usage
	{
		CLIENT_USE, 	  /// Context is used by a client.
		SERVER_USE,       /// Context is used by a server.
		TLSV1_CLIENT_USE, /// Context is used by a client requiring TLSv1.
		TLSV1_SERVER_USE  /// Context is used by a server requiring TLSv2.
	};
	
	enum VerificationMode 
	{
		VERIFY_NONE    = SSL_VERIFY_NONE, 
			/// Server: The server will not send a client certificate 
			/// request to the client, so the client will not send a certificate. 
			///
			/// Client: If not using an anonymous cipher (by default disabled), 
			/// the server will send a certificate which will be checked, but
			/// the result of the check will be ignored.
			
		VERIFY_RELAXED = SSL_VERIFY_PEER, 
			/// Server: The server sends a client certificate request to the 
			/// client. The certificate returned (if any) is checked. 
			/// If the verification process fails, the TLS/SSL handshake is 
			/// immediately terminated with an alert message containing the 
			/// reason for the verification failure. 
			///
			/// Client: The server certificate is verified, if one is provided. 
			/// If the verification process fails, the TLS/SSL handshake is
			/// immediately terminated with an alert message containing the 
			/// reason for the verification failure. 
			
		VERIFY_STRICT  = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			/// Server: If the client did not return a certificate, the TLS/SSL 
			/// handshake is immediately terminated with a handshake failure
			/// alert. 
			///
			/// Client: Same as VERIFY_RELAXED. 
			
		VERIFY_ONCE    = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE
			/// Server: Only request a client certificate on the initial 
			/// TLS/SSL handshake. Do not ask for a client certificate 
			/// again in case of a renegotiation.
			///
			/// Client: Same as VERIFY_RELAXED.	
	};

	SSLContext(
		Usage usage,
		const std::string& privateKeyFile,
		const std::string& certificateFile,
		const std::string& caLocation, 
		VerificationMode verificationMode = VERIFY_RELAXED,
		int verificationDepth = 9,
		bool loadDefaultCAs = false,
		const std::string& cipherList = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			/// Creates a Context.
			/// 
			///   * usage specifies whether the context is used by a client or server.
			///   * privateKeyFile contains the path to the private key file used for encryption.
			///     Can be empty if no private key file is used.
			///   * certificateFile contains the path to the certificate file (in PEM format).
			///     If the private key and the certificate are stored in the same file, this
			///     can be empty if privateKeyFile is given.
			///   * caLocation contains the path to the file or directory containing the
			///     CA/root certificates. Can be empty if the OpenSSL builtin CA certificates
			///     are used (see loadDefaultCAs).
			///   * verificationMode specifies whether and how peer certificates are validated.
			///   * verificationDepth sets the upper limit for verification chain sizes. Verification
			///     will fail if a certificate chain larger than this is encountered.
			///   * loadDefaultCAs specifies wheter the builtin CA certificates from OpenSSL are used.
			///   * cipherList specifies the supported ciphers in OpenSSL notation.
			///
			/// Note: If the private key is protected by a passphrase, a PrivateKeyPassphraseHandler
			/// must have been setup with the SSLManager, or the SSLManager's PrivateKeyPassphraseRequired
			/// event must be handled.

	SSLContext(
		Usage usage,
		const std::string& caLocation, 
		VerificationMode verificationMode = VERIFY_RELAXED,
		int verificationDepth = 9,
		bool loadDefaultCAs = false,
		const std::string& cipherList = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			/// Creates a Context.
			/// 
			///   * usage specifies whether the context is used by a client or server.
			///   * caLocation contains the path to the file or directory containing the
			///     CA/root certificates. Can be empty if the OpenSSL builtin CA certificates
			///     are used (see loadDefaultCAs).
			///   * verificationMode specifies whether and how peer certificates are validated.
			///   * verificationDepth sets the upper limit for verification chain sizes. Verification
			///     will fail if a certificate chain larger than this is encountered.
			///   * loadDefaultCAs specifies wheter the builtin CA certificates from OpenSSL are used.
			///   * cipherList specifies the supported ciphers in OpenSSL notation.
			///
			/// Note that a private key and/or certificate must be specified with
			/// usePrivateKey()/useCertificate() before the Context can be used.

	~SSLContext();
		/// Destroys the Context.

	void useCertificate(const Poco::Crypto::X509Certificate& certificate);
		/// Sets the certificate to be used by the Context.
		///
		/// To set-up a complete certificate chain, it might be
		/// necessary to call addChainCertificate() to specify
		/// additional certificates.
		///
		/// Note that useCertificate() must always be called before
		/// usePrivateKey().
		
	void addChainCertificate(const Poco::Crypto::X509Certificate& certificate);
		/// Adds a certificate for certificate chain validation.
		
	void usePrivateKey(const Poco::Crypto::RSAKey& key);
		/// Sets the private key to be used by the Context.
		///
		/// Note that useCertificate() must always be called before
		/// usePrivateKey().
		///
		/// Note: If the private key is protected by a passphrase, a PrivateKeyPassphraseHandler
		/// must have been setup with the SSLManager, or the SSLManager's PrivateKeyPassphraseRequired
		/// event must be handled.

	SSL_CTX* sslContext() const;
		/// Returns the underlying OpenSSL SSL Context object.

	Usage usage() const;
		/// Returns whether the context is for use by a client or by a server
		/// and whether TLSv1 is required.
		
	bool isForServerUse() const;
		/// Returns true if the context is for use by a server.

	SSLContext::VerificationMode verificationMode() const;
		/// Returns the verification mode.
		
	void enableSessionCache(bool flag = true);
		/// Enable or disable SSL/TLS session caching.
		/// For session caching to work, it must be enabled
		/// on the server, as well as on the client side.
		///
		/// The default is disabled session caching.
		///
		/// To enable session caching on the server side, use the
		/// two-argument version of this method to specify
		/// a session ID context.

	void enableSessionCache(bool flag, const std::string& sessionIdContext);
		/// Enables or disables SSL/TLS session caching on the server.
		/// For session caching to work, it must be enabled
		/// on the server, as well as on the client side.
		///
		/// SessionIdContext contains the application's unique
		/// session ID context, which becomes part of each
		/// session identifier generated by the server within this
		/// context. SessionIdContext can be an arbitrary sequence 
		/// of bytes with a maximum length of SSL_MAX_SSL_SESSION_ID_LENGTH.
		///
		/// A non-empty sessionIdContext should be specified even if
		/// session caching is disabled to avoid problems with clients
		/// requesting to reuse a session (e.g. Firefox 3.6).
		///
		/// This method may only be called on SERVER_USE Context objects.
		
	bool sessionCacheEnabled() const;
		/// Returns true if the session cache is enabled.
		
	void setSessionCacheSize(std::size_t size);
		/// Sets the maximum size of the server session cache, in number of
		/// sessions. The default size (according to OpenSSL documentation)
		/// is 1024*20, which may be too large for many applications,
		/// especially on embedded platforms with limited memory.
		///
		/// Specifying a size of 0 will set an unlimited cache size.
		///
		/// This method may only be called on SERVER_USE Context objets.
		
	std::size_t getSessionCacheSize() const;
		/// Returns the current maximum size of the server session cache.
		///
		/// This method may only be called on SERVER_USE Context objets.
		
	void setSessionTimeout(long seconds);
		/// Sets the timeout (in seconds) of cached sessions on the server.
		/// A cached session will be removed from the cache if it has
		/// not been used for the given number of seconds.
		///
		/// This method may only be called on SERVER_USE Context objets.

	long getSessionTimeout() const;
		/// Returns the timeout (in seconds) of cached sessions on the server.
		///
		/// This method may only be called on SERVER_USE Context objets.

	void flushSessionCache();
		/// Flushes the SSL session cache on the server.
		///
		/// This method may only be called on SERVER_USE Context objets.
				
	void disableStatelessSessionResumption();
		/// Newer versions of OpenSSL support RFC 4507 tickets for stateless
		/// session resumption.
		///
		/// The feature can be disabled by calling this method.

private:
	void createSSLContext();
		/// Create a SSL_CTX object according to Context configuration.

	Usage _usage;
	VerificationMode _mode;
	SSL_CTX* _sslContext;
	bool _extendedCertificateVerification;
};


//
// Inlines
//


inline SSLContext::Usage SSLContext::usage() const
{
	return _usage;
}


inline bool SSLContext::isForServerUse() const
{
	return _usage == SERVER_USE || _usage == TLSV1_SERVER_USE;
}


inline SSLContext::VerificationMode SSLContext::verificationMode() const
{
	return _mode;
}


inline SSL_CTX* SSLContext::sslContext() const
{
	return _sslContext;
}


//
// Utilities
//


inline SSLContext::VerificationMode convertVerificationMode(const std::string& vMode)
	/// Non-case sensitive conversion of a string to a VerificationMode enum.
	/// If verMode is illegal an InvalidArgumentException is thrown.
{
	std::string mode = Poco::toLower(vMode);
	SSLContext::VerificationMode verMode = SSLContext::VERIFY_STRICT;

	if (mode == "none")
		verMode = SSLContext::VERIFY_NONE;
	else if (mode == "relaxed")
		verMode = SSLContext::VERIFY_RELAXED;
	else if (mode == "strict")
		verMode = SSLContext::VERIFY_STRICT;
	else if (mode == "once")
		verMode = SSLContext::VERIFY_ONCE;
	else
		throw Exception("Invalid verification mode. Should be relaxed, strict or once but got", vMode);

	return verMode;
}


inline std::string convertCertificateError(long errCode)
	/// Converts an SSL certificate handling error code into an error message.
{
	std::string errMsg(X509_verify_cert_error_string(errCode));
	return errMsg;
}


inline std::string getLastError()
	/// Returns the last error from the error stack
{
	unsigned long errCode = ERR_get_error();
	if (errCode != 0)
	{
		char buffer[256];
		ERR_error_string_n(errCode, buffer, sizeof(buffer));
		return std::string(buffer);
	}
	else return "No error";
}


inline void clearErrorStack()
	/// Clears the error stack
{
	ERR_clear_error();
}


} } // namespace scy::net


#endif // SOURCEY_Net_SSLContext_H