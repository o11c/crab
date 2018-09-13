from crab.crab import CrabFile, CrabSection, CrabPurpose, CRAB_SCHEMA

import gc
import unittest


def nspd_tuple(s):
    return (s.number(), s.schema(), s.purpose(), s.data()[:])

def cat_bytes(*strings):
    bits = []
    for s in strings:
        bits.append(s.encode('utf-8'))
        bits.append(b'\0')
    return b''.join(bits)

class TestCrabFile(unittest.TestCase):
    def assertContentsEqual(self, an, bn):
        with open(an, 'rb') as af, open(bn, 'rb') as bf:
            ac = af.read()
            bc = bf.read()
        self.assertEqual(ac, bc)

    def test_empty(self):
        c = CrabFile('tmp/empty.crab', new=True)
        c.save(reopen=False)
        c = None; gc.collect()
        self.assertContentsEqual('tmp/empty.crab', 'test-data/empty.crab')

        c = CrabFile('tmp/empty.crab')
        self.assertEqual(c.num_sections(), 2)
        s0 = c.section(0)
        s1 = c.section(1)

        self.assertEqual(s0.schema(), CRAB_SCHEMA)
        self.assertEqual(s0.purpose(), CrabPurpose.Schema)
        d0 = s0.data()[:]
        self.assertEqual(len(d0), 16)
        self.assertEqual(d0,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x01' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
        )

        self.assertEqual(s1.schema(), CRAB_SCHEMA)
        self.assertEqual(s1.purpose(), CrabPurpose.Supplementary)
        d1 = s1.data()[:]
        self.assertEqual(len(d1), 40)
        self.assertEqual(d1, cat_bytes(CRAB_SCHEMA))
        c.close()

    def test_hello(self):
        c = CrabFile('tmp/hello.crab', new=True)

        s2 = c.add_section()
        assert s2.number() == 2
        s2.set_schema_and_purpose(CRAB_SCHEMA, CrabPurpose.Raw)
        s2.set_data(b'Hello, World!\n')
        self.assertEqual(c.num_sections(), 3)
        s0 = c.section(0)
        s1 = c.section(1)
        self.assertEqual(nspd_tuple(s0), (0, CRAB_SCHEMA, CrabPurpose.Schema,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x01' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
        ))
        self.assertEqual(nspd_tuple(s1), (1, CRAB_SCHEMA, CrabPurpose.Supplementary,
                cat_bytes(CRAB_SCHEMA)))
        self.assertEqual(nspd_tuple(s2), (2, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))

        c.save(reopen=True)
        # check section pointer stability
        assert s0 == c.section(0)
        assert s1 == c.section(1)
        assert s2 == c.section(2)

        s3 = c.add_section()
        assert s3.number() == 3
        s3.set_schema_and_purpose(CRAB_SCHEMA, CrabPurpose.Raw)
        s3.set_data(b'Hello, World!\n')
        s4 = c.add_section()
        assert s4.number() == 4
        s4.set_schema_and_purpose('bogus:whatever', 5)
        self.assertEqual(nspd_tuple(s0), (0, CRAB_SCHEMA, CrabPurpose.Schema,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x02' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
                b'\x00\x00\x28\x0e' # schemas[1].url (offset 0, length 14)
                b'\x00\x00\x00\x00' # schemas[1].reserved
        ))
        self.assertEqual(nspd_tuple(s1), (1, CRAB_SCHEMA, CrabPurpose.Supplementary,
                cat_bytes(CRAB_SCHEMA, 'bogus:whatever')))
        self.assertEqual(nspd_tuple(s2), (2, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s3), (3, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s4), (4, 'bogus:whatever', 5,
                b''))

        s3.set_schema_and_purpose('bogus:something-else', 6)
        self.assertEqual(nspd_tuple(s0), (0, CRAB_SCHEMA, CrabPurpose.Schema,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x03' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
                b'\x00\x00\x28\x0e' # schemas[1].url (offset 40, length 14)
                b'\x00\x00\x00\x00' # schemas[1].reserved
                b'\x00\x00\x37\x14' # schemas[2].url (offset 55, length 20)
                b'\x00\x00\x00\x00' # schemas[2].reserved
        ))
        self.assertEqual(nspd_tuple(s1), (1, CRAB_SCHEMA, CrabPurpose.Supplementary,
                cat_bytes(CRAB_SCHEMA, 'bogus:whatever', 'bogus:something-else')))
        self.assertEqual(nspd_tuple(s2), (2, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s3), (3, 'bogus:something-else', 6,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s4), (4, 'bogus:whatever', 5,
                b''))

        with open('test-data/random.bin', 'rb') as f:
            random_data = f.read()
        s4.set_data(random_data)
        self.assertEqual(nspd_tuple(s0), (0, CRAB_SCHEMA, CrabPurpose.Schema,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x03' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
                b'\x00\x00\x28\x0e' # schemas[1].url (offset 40, length 14)
                b'\x00\x00\x00\x00' # schemas[1].reserved
                b'\x00\x00\x37\x14' # schemas[2].url (offset 55, length 20)
                b'\x00\x00\x00\x00' # schemas[2].reserved
        ))
        self.assertEqual(nspd_tuple(s1), (1, CRAB_SCHEMA, CrabPurpose.Supplementary,
                cat_bytes(CRAB_SCHEMA, 'bogus:whatever', 'bogus:something-else')))
        self.assertEqual(nspd_tuple(s2), (2, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s3), (3, 'bogus:something-else', 6,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s4), (4, 'bogus:whatever', 5,
                random_data))

        s3.set_schema_and_purpose(CRAB_SCHEMA, 0)
        s3.set_data(b'')
        self.assertEqual(nspd_tuple(s0), (0, CRAB_SCHEMA, CrabPurpose.Schema,
                b'\x00\x00\x00\x01' # string_section
                b'\x00\x00' # reserved
                b'\x00\x03' # num_schemas
                b'\x00\x00\x00\x27' # schemas[0].url (offset 0, length 39)
                b'\x00\x00\x00\x00' # schemas[0].reserved
                b'\x00\x00\x28\x0e' # schemas[1].url (offset 40, length 14)
                b'\x00\x00\x00\x00' # schemas[1].reserved
                b'\x00\x00\x37\x14' # schemas[2].url (offset 55, length 20)
                b'\x00\x00\x00\x00' # schemas[2].reserved
        ))
        self.assertEqual(nspd_tuple(s1), (1, CRAB_SCHEMA, CrabPurpose.Supplementary,
                cat_bytes(CRAB_SCHEMA, 'bogus:whatever', 'bogus:something-else')))
        self.assertEqual(nspd_tuple(s2), (2, CRAB_SCHEMA, CrabPurpose.Raw,
                b'Hello, World!\n'))
        self.assertEqual(nspd_tuple(s3), (3, CRAB_SCHEMA, 0,
                b''))
        self.assertEqual(nspd_tuple(s4), (4, 'bogus:whatever', 5,
                random_data))

        c.save(reopen=False)
        c.close()
        self.assertContentsEqual('tmp/hello.crab', 'test-data/hello.crab')
