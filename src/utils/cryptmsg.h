#include "crypto/crypto.h"
#include <vector>

namespace graft::crypto_tools {

/*!
 * \brief encryptMessage - encrypts data for recipients using their B public keys (assumed public view keys).
 *
 * \param input - data to encrypt.
 * \param Bkeys - vector of B keys for each recipients.
 * \param output - resulting encripted message.
 */
void encryptMessage(const std::string& input, const std::vector<crypto::public_key>& Bkeys, std::string& output);

/*!
 * \brief encryptMessage - encrypts data for single recipient using B public keys (assumed public view key).
 *
 * \param input - data to encrypt.
 * \param Bkey - B keys of recipient.
 * \param output - resulting encripted message.
 */
void encryptMessage(const std::string& input, const crypto::public_key& Bkey, std::string& output);

/*!
 * \brief decryptMessage - (reverse of encryptForBs) decrypts data for one of the recipients using his b secret key.
 *
 * \param input - data that was created by encryptForBs.
 * \param bkey - secret key corresponding to one of Bs that were used to encrypt.
 * \param output - resulting decrypted data.
 * \return true on success or false otherwise
 */
bool decryptMessage(const std::string& input, const crypto::secret_key& bkey, std::string& output);

} //namespace graft::crypto_tools
