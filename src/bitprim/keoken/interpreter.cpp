/**
 * Copyright (c) 2017-2018 Bitprim Inc.
 *
 * This file is part of Bitprim.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <bitprim/keoken/interpreter.hpp>

#include <bitcoin/bitcoin/chain/output.hpp>

using libbitcoin::chain::transaction;
using libbitcoin::chain::output;
using libbitcoin::data_chunk;
using libbitcoin::reader;
using libbitcoin::wallet::payment_address;

namespace bitprim {
namespace keoken {

bool interpreter::version_dispatcher(size_t block_height, transaction const& tx, reader& source) {

    auto version = source.read_2_bytes_big_endian();
    if ( ! source) return false;

    switch (version) {
        case 0x00:      //TODO(fernando): enum
            return version_0_type_dispatcher(block_height, tx, source);
    }

    return false;
}

bool interpreter::version_0_type_dispatcher(size_t block_height, transaction const& tx, reader& source) {
    auto type = source.read_2_bytes_big_endian();
    if ( ! source) return false;

    switch (type) {
        case 0x00:      //TODO(fernando): enum, create asset 
            return process_create_asset_version_0(block_height, tx, source);
        case 0x01:
            return process_send_tokens_version_0(block_height, tx, source);
    }
    return false;
}

payment_address interpreter::get_first_input_addr(transaction const& tx) const {
    auto const& owner_input = tx.inputs()[0];

    output out_output;
    size_t out_height;
    uint32_t out_median_time_past;
    bool out_coinbase;

    if ( ! fast_chain_.get_output(out_output, out_height, out_median_time_past, out_coinbase, 
                    owner_input.previous_output(), libbitcoin::max_size_t, true)) {
        return payment_address{};   //TODO: check if it has is_valid()
    }

    return out_output.address();
}

bool interpreter::process_create_asset_version_0(size_t block_height, transaction const& tx, reader& source) {
    auto msg = message::create_asset::factory_from_data(source);
    if ( ! source) return false;    //TODO: error codes

    if (msg.amount() <= 0) {
        return false;               //TODO: error codes
    }

    auto const owner = get_first_input_addr(tx);
    if ( ! owner) {
        return false;               //TODO: error codes
    }

    state_.create_asset(msg, owner, block_height, tx.hash());

    return true;
}


std::pair<payment_address, payment_address> interpreter::get_send_tokens_addrs(transaction const& tx) const {
    auto source = get_first_input_addr(tx);
    if ( ! source) {
        return {payment_address{}, payment_address{}};
    }

    auto it = std::find_if(tx.outputs().begin(), tx.outputs().end(), [&source](output const& o) {
        return o.address() && o.address() != source;
    });

    if (it == tx.outputs().end()) {
        return {payment_address{}, payment_address{}};        
    }

    return {source, it->address()};
}


bool interpreter::process_send_tokens_version_0(size_t block_height, transaction const& tx, reader& source) {
    auto msg = message::send_tokens::factory_from_data(source);
    if ( ! source) return false;

    if ( ! state_.asset_id_exists(msg.asset_id())) {
        return false;
    }

    if (msg.amount() <= 0) {
        return false;               //TODO: error codes
    }
  
    auto wallets = get_send_tokens_addrs(tx);
    payment_address const& source_addr = wallets.first;
    payment_address const& target_addr = wallets.second;

    if ( ! source_addr || ! target_addr) {
        return false;               //TODO: error codes
    }

    if (state_.get_balance(msg.asset_id(), source_addr) < msg.amount()) {
        return false;               //TODO: error codes
    }

    state_.create_balance_entry(msg, source_addr, target_addr, block_height, tx.hash());

    return true;
}

// void interpreter::extract_keoken_info(size_t from_block) {
//     chain::block b;

//     for each block in the blockchain from the genesis keoken block {
//         for_each_keoken_tx(b, [](size_t block_height, transaction const& tx, data_chunk const& keo_data) {
//             istream_reader source(data_source(keo_data));
//             version_dispatcher(block_height, tx, source);
//         });
//     }
// }

} // namespace keoken
} // namespace bitprim












// STATUS? get_global_status() {

//     std::vector<chain::block> blockchain;

//     for (auto const& b : blockchain) {

//         for_each_keoken_tx(b, [](size_t block_height, transaction const& tx, data_chunk const& keo_data) {
//             version = keo_data[0] + keo_data[1];    //TODO(fernando): replace it
//             hace_algo_con_version(block_height, tx, version, keo_data);
//         });

//     }
// }


/*
Formatos:
    TX Asset Creation (completa a nivel BCH):
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ...
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
        ...
id:1    output j: OP_RETURN 00KP version:0, type:0, name: Piticoin, amount: 100
                  Este output es el que se procesa a nivel Keoken
        ...
        output n: OP_RETURN 00KP version:0, type:0, name: Wandacoin, amount: 200
                  Ignorada
        ...

        Que pasa si el amount es cero?

    TX Sends Token (completa a nivel BCH):
        input 0: tiene guita y manda (la wallet de prev out tiene saldo disponible en el asset)
                 Unlocking script (para desbloquear la guita de los fees)
                 Tiene BCH
                 Tiene Saldo del Asset
        input 1: Puede haber un segundo input, para intencambiar BCH...
        input 2: Puede haber un segundo input Keoken????
        ----------------------------------------------------------------------
        output 0 -> target: wallet que recibe los tokens, bch amount: dust
        output 1 -> puede ser vuelto, solamente consideradas a nivel BCH
        output 2 -> OP_RETURN 00KP version:0, type:1, id: 1, amount: 10
        output 3 -> puede ser vuelto, solamente consideradas a nivel BCH

        Que pasa si el amount es cero?









    TX BCH Previa:
        input 0: - ???
        ----------------------------------------------------------------------
        output 0 -> amount, Locking Script #1

    TX BCH Normal:
        input 0: - previous output: txid: hash, index: nro del output dentro de la txid
                 - unlocking script (script que desbloquea el locking script que está en el prevout)
                 - Unlocking Script #1 que matchea con Locking Script #1
        ----------------------------------------------------------------------
        output 0 -> ???



Ejemplos:

    TX xxx Sends Token
        input 0: tiene guita y manda (la wallet de prev out tiene saldo disponible en el asset)
        ----------------------------------------------------------------------
        output 0 -> tiene la misma wallet que input0
        output 1 -> OP_RETURN 00KP version:0, type:1, id: 1, amount: 10
        Mueve la guita a la misma cuenta,
        no vale la pena crear un registro en el balance por esto...

    TX xxx Sends Token
        input 0: tiene guita y manda (la wallet de prev out tiene saldo disponible en el asset)
        ----------------------------------------------------------------------
        output 0 -> OP_RETURN 00KP version:0, type:1, id: 1, amount: 10
        output 1 -> una wallet distinta a la input0



    TX 0, Asset Creation, Non-Standard BCH Transaction
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
id:n    output j: OP_RETURN 00KP version:0, type:0, name: Piticoin, amount: 100
                  Este output es el que se procesa a nivel Keoken
        output n: OP_RETURN 00KP version:0, type:0, name: Wandacoin, amount: 200
                  Ignorada, TX non-standard, Keoken rule.
                  Keoken procesa transacciones standard y no-standard, pero en el caso
                  de que la TX sea no-estandar sólo vamos a procesar el primer 
                  Keoken-output (OP-RETURN 00KP)
        ...

    TX 1, Asset Creation, Standard BCH Transaction
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ...
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
        ...
id:n+1  output n: OP_RETURN 00KP version:0, type:0, name: Wandacoin, amount: 200
        ...

    TX 2, Invalid Keoken, Standard BCH Transaction
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ...
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
        ...
        output n: OP_RETURN 00KP FRUTA
                  Ignorada
        ...

    TX 3, Invalid Keoken, Standard BCH Transaction
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ...
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
        ...
        output n: OP_RETURN 00KP version:0, type:0 ... falta el resto
                  Ignorada
        ...

    TX 4, Asset Creation, Standard BCH Transaction
        input 0: el wallet de este input es el owner de los tokens creados
                 paga guita de los fees   
        input 1: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ...
        input n: ignorada a nivel keoken, paga guita de los fees u otra cosa
        ----------------------------------------------------------------------
        output 0: ignorada a nivel Keoken
        output 1: ignorada a nivel Keoken
        ...
id:n+2  output n: OP_RETURN 00KP version:0, type:0, name: EhAmigo, amount: 300


*/



/*
API:

    - crear un asset                           (Guille y Rama)
        Input:

        Acciones:
        - crear un objeto keoken interno
        - crea la TX con
        - envia la TX a la red (organize)

    - send tokens                               (Guille y Rama)
        Input:
        Acciones:
        - crear un objeto keoken interno
        - crea la TX con
        - envia la TX a la red (organize)

    - listar todos los assets de la blockchain 
        - nombre de asset
        - cantidad (que es la que se creó)
        - id (generado con el conteo)
        - dueno
        - block height donde se creó
        - Tx donde se creó

    - balance en token para un address (de todos los assets del address)
        - nombre de asset
        - cantidad (que es la que se creó)
        - id (generado con el conteo)
        - balance en tokens

    - cuenta corriente (balance con detalle) en tokens para un address (de todos los assets del address)
        - nombre de asset
        - cantidad (que es la que se creó)
        - id (generado con el conteo)
        - detalle de TXs que mueven

    - 

*/    

/*
C# 
interface IGlobalStateReader {
    IEnumerable<X> GetAssets();
    IEnumerable<Y> GetBalanceDetails();
}

interface IGlobalStateWriter {
    CreateAsset(X);
    CreateBalanceLine(Y);
}

interface IGlobalStateMutate : IGlobalStateReader, IGlobalStateWriter
{}

class GlobalData : IGlobalStateMutate {
    IEnumerable<X> GetAssets() {
        sincronize1
    }

    IEnumerable<Y> GetBalanceDetails() {
        sincronize2
    }

    CreateAsset(X) {
        sinchronize1
    }

    CreateBalanceLine(Y) {
        sinchronize2
    }    
}

class Proccessor {
    IGlobalStateMutate data;

    Proccessor(IGlobalStateMutate data) {
        data = data;
    }

    void RecorroBlockchainYActualizoData() {
        data.CreateAsset(...);
    }
}
*/

/*
concept IGlobalStateReader {
    IEnumerable<X> GetAssets();
    IEnumerable<Y> GetBalanceDetails();
}

concept IGlobalStateWriter {
    CreateAsset(X);
    CreateBalanceLine(Y);
}

concept IGlobalStateMutate {
    IGlobalStateReader && IGlobalStateWriter
}

class GlobalData {
    IEnumerable<X> GetAssets() {
        sincronize1
    }

    IEnumerable<Y> GetBalanceDetails() {
        sincronize2
    }

    CreateAsset(X) {
        sinchronize1
    }

    CreateBalanceLine(Y) {
        sinchronize2
    }    
}

class PepeData {
    IEnumerable<X> GetPepe() {
        sincronize1
    }
}

template <GlobalStateMutate G>
class Processor {
    G data_;

    Processor(G data) : data_(data) {}
};

template <typename G> //unconstraintned, concept implicito GlobalStateMutate
class Processor2 {
    G data_;

    Processor(G data) : data_(data) {}

    void DoSomething() {
        data_.CreateAsset(...);// <<<<<
    }
};

GlobalData gd;                      //ok
Processor<GlobalData> p1(gd);       //ok

PepeData pd;                       //ok
Processor<PepeData> p2(pd);       //error, PepeData is not a GlobalStateMutate

PepeData pd;                       //ok
Processo2r<PepeData> p2(pd);       //ok
p2.DoSomething();                   // error en <<<<< PepeData no tiene CreateAsset
*/


