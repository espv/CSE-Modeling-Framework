#include "cc2420-trailer.h"


namespace ns3
{
    NS_OBJECT_ENSURE_REGISTERED (CC2420Trailer);

    uint16_t CC2420Trailer::crc_tabccitt[256];

    bool CC2420Trailer::crc_tabccitt_init = false;
    CC2420Trailer::CC2420Trailer()
    {
        // we must provide a public default constructor,
        // implicit or explicit, but never private.
    }

    CC2420Trailer::~CC2420Trailer()
    {
    }

    TypeId CC2420Trailer::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CC2420Trailer").SetParent<Trailer>().AddConstructor<CC2420Trailer>();
        return tid;
    }

    TypeId CC2420Trailer::GetInstanceTypeId(void) const
    {
        return GetTypeId();
    }

    void CC2420Trailer::Print(std::ostream & os) const
    {
        // This method is invoked by the packet printing
        // routines to print the content of my header.
        os << "Frame Check Sequence=0x" << std::hex <<(uint32_t)(m_fcs) << std::dec;
    }

    uint32_t CC2420Trailer::GetSerializedSize(void) const
    {
        // Frame Check Sequence = 2 Bytes
        return 2;
    }

    void CC2420Trailer::Serialize(Buffer::Iterator end) const
    {
        end.Prev(GetSerializedSize());
        end.WriteU16(m_fcs);
    }

    uint32_t CC2420Trailer::Deserialize(Buffer::Iterator end)
    {
        end.Prev(GetSerializedSize());
        //read Frame Check Sequence form last two Byte
        m_fcs = end.ReadU16();
        // we return the number of bytes effectively read.
        return GetSerializedSize();
    }

    void CC2420Trailer::SetFcs(uint16_t fcs)
    {
        m_fcs = fcs;
    }

    void CC2420Trailer::CalcFcs(Ptr<const Packet> p)
    {
        int len = p->GetSize();
        uint8_t *buffer;
        buffer = new uint8_t[len];
        p->CopyData(buffer, len);
        m_fcs = DoCalcFcs(buffer, len);
        delete [] buffer;
    }

    uint16_t CC2420Trailer::GetFcs()
    {
        return m_fcs;
    }

    bool CC2420Trailer::CheckFcs(Ptr<const Packet> p) const
    {
        int len = p->GetSize();
        uint8_t *buffer;
        uint32_t crc;
        buffer = new uint8_t[len];
        p->CopyData(buffer, len);
        crc = DoCalcFcs(buffer, len);
        delete [] buffer;
        return (m_fcs == crc);
    }

    uint16_t CC2420Trailer::DoCalcFcs(const uint8_t *buffer, size_t len) const
    {
        uint16_t crc = 0;
        while(len--){
            crc = update_crc_ccitt(crc, *(buffer++));
        }
        return crc;
    }

    /*******************************************************************\
    *                                                                   *
    *   unsigned short update_crc_ccitt( unsigned long crc, char c );   *
    *                                                                   *
    *   The function update_crc_ccitt calculates  a  new  CRC-CCITT     *
    *   value  based  on the previous value of the CRC and the next     *
    *   byte of the data to be checked.                                 *
    *                                                                   *
    \*******************************************************************/
    uint16_t CC2420Trailer::update_crc_ccitt(uint16_t crc, uint8_t c) const
    {
        uint16_t tmp, short_c;
        short_c = 0x00ff & (uint16_t)(c);
        if(!crc_tabccitt_init)
            init_crcccitt_tab();

        tmp = (crc >> 8) ^ short_c;
        crc = (crc << 8) ^ crc_tabccitt[tmp];
        return crc;
    } /* update_crc_ccitt */
    void CC2420Trailer::init_crcccitt_tab(void)
    {
        uint16_t i, j;
        uint16_t crc, c;
        for(i = 0;i < 256;i++){
            crc = 0;
            c = ((uint16_t)(i)) << 8;
            for(j = 0;j < 8;j++){
                if((crc ^ c) & 0x8000)
                    crc = (crc << 1) ^ 0x1021;

                else
                    crc = crc << 1;

                c = c << 1;
            }
            crc_tabccitt[i] = crc;
        }

        crc_tabccitt_init = true;
    }

    /* namespace ns3 */
}

