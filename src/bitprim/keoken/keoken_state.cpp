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



#include <bitcoin/bitcoin/chain/transaction.hpp>

using libbitcoin::data_chunk;

size_t asset_id = 1;

bool version_dispatcher(size_t block_height, chain::transaction const& tx, reader& source) {

    auto version = source.read_2_bytes_big_endian();
    if ( ! source) return false;

    switch (base.version()) {
        case 0x00:      //TODO(fernando): enum
            return version_0_type_dispatcher(block_height, tx, source);
    }

    return false;
}

bool version_0_type_dispatcher(size_t block_height, chain::transaction const& tx, reader& source) {
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

bool process_create_asset_version_0(size_t block_height, chain::transaction const& tx, reader& source) {
    auto msg = message::create_asset::factory_from_data(source);
    if ( ! source) return false;
    
    domain::asset obj(asset_id, obj.name(), obj.amount());
    ++asset_id;

    asset_list.emplace_back(block_height, tx, obj);

    //put obj in GLOBAL STATE
    //registro de global status: 
    //      - block_height
    //      - tx
    //      - domain::asset

    // balance
    //    key: {asset_id, wallet}, std::vector { {+obj.amount(), block_height, tx} }

    // Key                  Value
    // {Piticoin, Mario}:   [{100, 567890, alsjdalksdjalkdjad}];
}

bool process_send_tokens_version_0(..., tx, ...) {
    auto msg = message::send_tokens::factory_from_data(source);
    if ( ! source) return false;

    // check if msg.asset_id() exists in asset_list
    msg.asset_id();
    msg.amount();

    {source, target} = get_wallets(tx);  //podria haber error

    sincronizar {    
        saldo = get_saldo({msg.asset_id(), source})
        if (saldo <msg.amount()) return false;

        balance[{msg.asset_id(), source}] += {-msg.amount(), 567890, alsjdalksdjalkdjad};
        balance[{msg.asset_id(), target}] += { msg.amount(), 567890, alsjdalksdjalkdjad};
    }
}


void extract_keoken_info(size_t from_block) {
    chain::block b;

    for each block in the blockchain from the genesis keoken block {
        for_each_keoken_tx(b, [](size_t block_height, chain::transaction const& tx, data_chunk const& keo_data) {
            istream_reader source(data_source(keo_data));
            version_dispatcher(block_height, tx, source);
        });
    }
}


STATUS? get_global_status() {

    std::vector<chain::block> blockchain;

    for (auto const& b : blockchain) {

        for_each_keoken_tx(b, [](size_t block_height, chain::transaction const& tx, data_chunk const& keo_data) {
            version = keo_data[0] + keo_data[1];    //TODO(fernando): replace it
            hace_algo_con_version(block_height, tx, version, keo_data);
        });

    }
}




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


} // namespace keoken
} // namespace bitprim
