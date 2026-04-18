with open("examples/CMakeLists.txt", "w", encoding="utf-8") as f:
    f.write("""add_subdirectory(false_memory_convolution/FalseMemoryConvolution)
add_subdirectory(dynamization_haas_comb/DynamizationHaasComb)
add_subdirectory(phantom_sub_harmonic_exciter/PhantomSubHarmonicExciter)

add_subdirectory(oceanic_haas_widener/OceanicHaasWidener)
add_subdirectory(human_error_ensemble/HumanErrorEnsemble)
""")