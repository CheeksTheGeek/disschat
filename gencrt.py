from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from datetime import datetime, timedelta
import ipaddress

def generate_self_signed_cert(hostname, ip_addresses=None, key_file="server.key", cert_file="server.crt"):
    # Generate our key
    key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=2048,
    )

    name = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, hostname)
    ])

    # Best practice is to use a separate issuer and subject name
    issuer = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, f"{hostname} CA")
    ])

    alt_names = [x509.DNSName(hostname)]
    if ip_addresses:
        for ip in ip_addresses:
            alt_names.append(x509.IPAddress(ipaddress.ip_address(ip)))

    cert = x509.CertificateBuilder().subject_name(
        name
    ).issuer_name(
        issuer
    ).public_key(
        key.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.utcnow()
    ).not_valid_after(
        # Our certificate will be valid for 1 year
        datetime.utcnow() + timedelta(days=365)
    ).add_extension(
        x509.SubjectAlternativeName(alt_names),
        critical=False,
    ).sign(key, hashes.SHA256())

    # Write our key to disk for safe keeping
    with open(key_file, "wb") as f:
        f.write(key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption(),
        ))

    # Write our certificate out to disk.
    with open(cert_file, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))

# Usage
generate_self_signed_cert("localhost", ip_addresses=["127.0.0.1"])
