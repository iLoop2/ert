from pandas import DataFrame
from ert.enkf.export import GenKwCollector, SummaryCollector, GenDataCollector, SummaryObservationCollector, \
    GenDataObservationCollector, CustomKWCollector


class PlotDataGatherer(object):

    def __init__(self, dataGatherFunc, conditionFunc, refcaseGatherFunc=None, observationGatherFunc=None):
        super(PlotDataGatherer, self).__init__()

        self.__dataGatherFunction = dataGatherFunc
        self.__conditionFunction = conditionFunc
        self.__refcaseGatherFunction = refcaseGatherFunc
        self.__observationGatherFunction = observationGatherFunc

    def hasRefcaseGatherFunction(self):
        """ :rtype: bool """
        return self.__refcaseGatherFunction is not None

    def hasObservationGatherFunction(self):
        """ :rtype: bool """
        return self.__observationGatherFunction is not None

    def canGatherDataForKey(self, key):
        """ :rtype: bool """
        return self.__conditionFunction(key)

    def gatherData(self, ert, case, key):
        """ :rtype: pandas.DataFrame """
        if not self.canGatherDataForKey(key):
            raise UserWarning("Unable to gather data for key: %s" % key)

        return self.__dataGatherFunction(ert, case, key)

    def gatherRefcaseData(self, ert, key):
        """ :rtype: pandas.DataFrame """
        if not self.canGatherDataForKey(key) or not self.hasRefcaseGatherFunction():
            raise UserWarning("Unable to gather refcase data for key: %s" % key)

        return self.__refcaseGatherFunction(ert, key)

    def gatherObservationData(self, ert, case, key):
        """ :rtype: pandas.DataFrame """
        if not self.canGatherDataForKey(key) or not self.hasObservationGatherFunction():
            raise UserWarning("Unable to gather observation data for key: %s" % key)

        return self.__observationGatherFunction(ert, case, key)


    @staticmethod
    def gatherGenKwData(ert, case, key):
        """ :rtype: pandas.DataFrame """
        data = GenKwCollector.loadAllGenKwData(ert, case, [key])
        return data[key].dropna()

    @staticmethod
    def gatherSummaryData(ert, case, key):
        """ :rtype: pandas.DataFrame """
        data = SummaryCollector.loadAllSummaryData(ert, case, [key])
        if not data.empty:
            data = data.reset_index()
            data = data.pivot(index="Date", columns="Realization", values=key)

        return data #.dropna()

    @staticmethod
    def gatherSummaryRefcaseData(ert, key):
        refcase = ert.eclConfig().getRefcase()

        if refcase is None or not key in refcase:
            return DataFrame()

        vector = refcase.get_vector(key, report_only=False)

        rows = []
        for index in range(1, len(vector)):
            node = vector[index]
            row = {
                "Date": node.date,
                key: node.value
            }
            rows.append(row)

        data = DataFrame(rows)
        data = data.set_index("Date")

        return data

    @staticmethod
    def gatherSummaryObservationData(ert, case, key):
        if ert.getKeyManager().isKeyWithObservations(key):
            return SummaryObservationCollector.loadObservationData(ert, case, [key]).dropna()
        else:
            return DataFrame()


    @staticmethod
    def gatherGenDataData(ert, case, key):
        """ :rtype: pandas.DataFrame """
        key, report_step = key.split("@", 1)
        report_step = int(report_step)
        try:
            data = GenDataCollector.loadGenData(ert, case, key, report_step)
        except ValueError:
            data = DataFrame()

        return data.dropna() # removes all rows that has a NaN


    @staticmethod
    def gatherGenDataObservationData(ert, case, key_with_report_step):
        """ :rtype: pandas.DataFrame """
        key, report_step = key_with_report_step.split("@", 1)
        report_step = int(report_step)

        obs_key = GenDataObservationCollector.getObservationKeyForDataKey(ert, key, report_step)

        if obs_key is not None:
            obs_data = GenDataObservationCollector.loadGenDataObservations(ert, case, obs_key)
            columns = {obs_key: key_with_report_step, "STD_%s" % obs_key: "STD_%s" % key_with_report_step}
            obs_data = obs_data.rename(columns=columns)
        else:
            obs_data = DataFrame()

        return obs_data.dropna()

    @staticmethod
    def gatherCustomKwData(ert, case, key):
        """ :rtype: pandas.DataFrame """
        data = CustomKWCollector.loadAllCustomKWData(ert, case, [key])[key]

        return data
