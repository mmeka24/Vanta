import streamlit as st

from vanta_hedge import __version__

st.set_page_config(page_title="Vanta Hedge")
st.title("Vanta Hedge")
st.caption(f"v{__version__}")
st.write("Protective-put hedge analyzer. Analysis workflow is not built yet.")
