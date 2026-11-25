"""Autenticação de administrador — protege painel de alarmes e e-mail.

Senha fixa do projeto: adm123
"""

SENHA_ADMIN = "adm123"


def verificar_senha(senha: str) -> bool:
    """Retorna True se a senha informada for a correta."""
    return senha == SENHA_ADMIN
